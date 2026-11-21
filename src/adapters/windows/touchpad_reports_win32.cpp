#include "touchpad_reports_win32.h"

// hidpi.h uses USAGE and NTSTATUS without including their definitions.
#include <hidusage.h>
#include <winternl.h>

#include <hidpi.h>

#include <new>
#include <vector>

static const USAGE REACH_HID_PAGE_GENERIC_DESKTOP = 0x01;
static const USAGE REACH_HID_PAGE_DIGITIZER = 0x0D;
static const USAGE REACH_HID_USAGE_X = 0x30;
static const USAGE REACH_HID_USAGE_Y = 0x31;
static const USAGE REACH_HID_USAGE_TIP_SWITCH = 0x42;
static const USAGE REACH_HID_USAGE_CONTACT_COUNT = 0x54;

static const size_t REACH_TOUCHPAD_DEVICE_CAPACITY = 4;
static const size_t REACH_TOUCHPAD_FINGER_LINK_CAPACITY = 16;

struct reach_touchpad_device
{
    HANDLE handle;
    std::vector<BYTE> preparsed;
    USHORT contact_count_link;
    USHORT finger_links[REACH_TOUCHPAD_FINGER_LINK_CAPACITY];
    size_t finger_link_count;
    float range_x;
    float range_y;
    int32_t usable;
};

struct reach_touchpad_reports
{
    reach_touchpad_device devices[REACH_TOUCHPAD_DEVICE_CAPACITY];
    size_t device_count;
    std::vector<BYTE> buffer;

    HANDLE frame_device;
    uint32_t frame_expected;
    uint32_t frame_read;
    uint32_t frame_touching;
    double frame_sum_x;
    double frame_sum_y;
    int32_t frame_open;
};

static PHIDP_PREPARSED_DATA reach_touchpad_preparsed(reach_touchpad_device *device)
{
    return reinterpret_cast<PHIDP_PREPARSED_DATA>(device->preparsed.data());
}

static int32_t reach_touchpad_value_cap_is(const HIDP_VALUE_CAPS &cap, USAGE page, USAGE usage)
{
    if (cap.UsagePage != page)
    {
        return 0;
    }
    return cap.IsRange ? usage >= cap.Range.UsageMin && usage <= cap.Range.UsageMax
                       : cap.NotRange.Usage == usage;
}

static void reach_touchpad_add_finger_link(reach_touchpad_device *device, USHORT link)
{
    for (size_t index = 0; index < device->finger_link_count; ++index)
    {
        if (device->finger_links[index] == link)
        {
            return;
        }
    }
    if (device->finger_link_count < REACH_TOUCHPAD_FINGER_LINK_CAPACITY)
    {
        device->finger_links[device->finger_link_count++] = link;
    }
}

static void reach_touchpad_describe_device(reach_touchpad_device *device)
{
    device->usable = 0;
    UINT size = 0;
    if (GetRawInputDeviceInfoW(device->handle, RIDI_PREPARSEDDATA, nullptr, &size) != 0 ||
        size == 0)
    {
        return;
    }
    device->preparsed.resize(size);
    if (GetRawInputDeviceInfoW(device->handle, RIDI_PREPARSEDDATA, device->preparsed.data(),
                               &size) == (UINT)-1)
    {
        return;
    }

    HIDP_CAPS caps = {};
    if (HidP_GetCaps(reach_touchpad_preparsed(device), &caps) != HIDP_STATUS_SUCCESS ||
        caps.NumberInputValueCaps == 0)
    {
        return;
    }
    std::vector<HIDP_VALUE_CAPS> values(caps.NumberInputValueCaps);
    USHORT value_count = caps.NumberInputValueCaps;
    if (HidP_GetValueCaps(HidP_Input, values.data(), &value_count,
                          reach_touchpad_preparsed(device)) != HIDP_STATUS_SUCCESS)
    {
        return;
    }

    int32_t have_contact_count = 0;
    for (USHORT index = 0; index < value_count; ++index)
    {
        const HIDP_VALUE_CAPS &cap = values[index];
        if (reach_touchpad_value_cap_is(cap, REACH_HID_PAGE_DIGITIZER,
                                        REACH_HID_USAGE_CONTACT_COUNT))
        {
            device->contact_count_link = cap.LinkCollection;
            have_contact_count = 1;
        }
        else if (reach_touchpad_value_cap_is(cap, REACH_HID_PAGE_GENERIC_DESKTOP,
                                             REACH_HID_USAGE_X))
        {
            reach_touchpad_add_finger_link(device, cap.LinkCollection);
            device->range_x = (float)(cap.LogicalMax - cap.LogicalMin);
        }
        else if (reach_touchpad_value_cap_is(cap, REACH_HID_PAGE_GENERIC_DESKTOP,
                                             REACH_HID_USAGE_Y))
        {
            device->range_y = (float)(cap.LogicalMax - cap.LogicalMin);
        }
    }
    device->usable = have_contact_count && device->finger_link_count > 0 &&
                     device->range_x > 0.0f && device->range_y > 0.0f;
}

static reach_touchpad_device *reach_touchpad_device_for(reach_touchpad_reports *reports,
                                                        HANDLE handle)
{
    for (size_t index = 0; index < reports->device_count; ++index)
    {
        if (reports->devices[index].handle == handle)
        {
            return &reports->devices[index];
        }
    }
    if (reports->device_count >= REACH_TOUCHPAD_DEVICE_CAPACITY)
    {
        return nullptr;
    }
    reach_touchpad_device *device = &reports->devices[reports->device_count++];
    device->handle = handle;
    reach_touchpad_describe_device(device);
    return device;
}

static void reach_touchpad_begin_frame(reach_touchpad_reports *reports, HANDLE device,
                                       uint32_t expected)
{
    reports->frame_device = device;
    reports->frame_expected = expected;
    reports->frame_read = 0;
    reports->frame_touching = 0;
    reports->frame_sum_x = 0.0;
    reports->frame_sum_y = 0.0;
    reports->frame_open = 1;
}

static int32_t reach_touchpad_finish_frame(reach_touchpad_reports *reports,
                                           const reach_touchpad_device *device,
                                           reach_touchpad_frame *out_frame)
{
    reports->frame_open = 0;
    out_frame->contact_count = reports->frame_touching;
    out_frame->centroid_x = reports->frame_touching > 0
                                ? (float)(reports->frame_sum_x / reports->frame_touching)
                                : 0.0f;
    out_frame->centroid_y = reports->frame_touching > 0
                                ? (float)(reports->frame_sum_y / reports->frame_touching)
                                : 0.0f;
    out_frame->range_x = device->range_x;
    out_frame->range_y = device->range_y;
    return 1;
}

static int32_t reach_touchpad_slot_touching(reach_touchpad_device *device, USHORT link,
                                            PCHAR report, ULONG length)
{
    USAGE usages[16] = {};
    ULONG usage_count = sizeof(usages) / sizeof(usages[0]);
    if (HidP_GetUsages(HidP_Input, REACH_HID_PAGE_DIGITIZER, link, usages, &usage_count,
                       reach_touchpad_preparsed(device), report, length) != HIDP_STATUS_SUCCESS)
    {
        return 0;
    }
    for (ULONG index = 0; index < usage_count; ++index)
    {
        if (usages[index] == REACH_HID_USAGE_TIP_SWITCH)
        {
            return 1;
        }
    }
    return 0;
}

/* Reads one HID report into the open frame. Returns 1 when that completes a frame. */
static int32_t reach_touchpad_read_report(reach_touchpad_reports *reports,
                                          reach_touchpad_device *device, PCHAR report, ULONG length,
                                          reach_touchpad_frame *out_frame)
{
    PHIDP_PREPARSED_DATA preparsed = reach_touchpad_preparsed(device);
    ULONG contact_count = 0;
    if (HidP_GetUsageValue(HidP_Input, REACH_HID_PAGE_DIGITIZER, device->contact_count_link,
                           REACH_HID_USAGE_CONTACT_COUNT, &contact_count, preparsed, report,
                           length) != HIDP_STATUS_SUCCESS)
    {
        return 0;
    }

    /* A non-zero count starts a frame; zero continues a hybrid frame, or reports all lifted. */
    if (contact_count > 0 || !reports->frame_open || reports->frame_device != device->handle)
    {
        reach_touchpad_begin_frame(reports, device->handle, contact_count);
    }
    if (reports->frame_expected == 0)
    {
        return reach_touchpad_finish_frame(reports, device, out_frame);
    }

    for (size_t slot = 0;
         slot < device->finger_link_count && reports->frame_read < reports->frame_expected; ++slot)
    {
        const USHORT link = device->finger_links[slot];
        ++reports->frame_read;
        if (!reach_touchpad_slot_touching(device, link, report, length))
        {
            continue;
        }
        ULONG x = 0;
        ULONG y = 0;
        if (HidP_GetUsageValue(HidP_Input, REACH_HID_PAGE_GENERIC_DESKTOP, link, REACH_HID_USAGE_X,
                               &x, preparsed, report, length) != HIDP_STATUS_SUCCESS ||
            HidP_GetUsageValue(HidP_Input, REACH_HID_PAGE_GENERIC_DESKTOP, link, REACH_HID_USAGE_Y,
                               &y, preparsed, report, length) != HIDP_STATUS_SUCCESS)
        {
            continue;
        }
        ++reports->frame_touching;
        reports->frame_sum_x += (double)x;
        reports->frame_sum_y += (double)y;
    }

    if (reports->frame_read >= reports->frame_expected)
    {
        return reach_touchpad_finish_frame(reports, device, out_frame);
    }
    return 0;
}

reach_touchpad_reports *reach_touchpad_reports_create(void)
{
    return new (std::nothrow) reach_touchpad_reports();
}

void reach_touchpad_reports_destroy(reach_touchpad_reports *reports)
{
    delete reports;
}

int32_t reach_touchpad_reports_feed(reach_touchpad_reports *reports, HRAWINPUT input,
                                    reach_touchpad_frame *out_frame)
{
    if (reports == nullptr || out_frame == nullptr)
    {
        return 0;
    }

    UINT size = 0;
    if (GetRawInputData(input, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0)
    {
        return 0;
    }
    reports->buffer.resize(size);
    if (GetRawInputData(input, RID_INPUT, reports->buffer.data(), &size, sizeof(RAWINPUTHEADER)) ==
        (UINT)-1)
    {
        return 0;
    }

    const RAWINPUT *raw = reinterpret_cast<const RAWINPUT *>(reports->buffer.data());
    if (raw->header.dwType != RIM_TYPEHID || raw->data.hid.dwSizeHid == 0)
    {
        return 0;
    }
    reach_touchpad_device *device = reach_touchpad_device_for(reports, raw->header.hDevice);
    if (device == nullptr || !device->usable)
    {
        return 0;
    }

    /* Several reports can arrive in one message; the last completed frame wins. */
    int32_t completed = 0;
    const DWORD report_size = raw->data.hid.dwSizeHid;
    for (DWORD index = 0; index < raw->data.hid.dwCount; ++index)
    {
        PCHAR report = reinterpret_cast<PCHAR>(const_cast<BYTE *>(raw->data.hid.bRawData)) +
                       (size_t)index * report_size;
        if (reach_touchpad_read_report(reports, device, report, report_size, out_frame))
        {
            completed = 1;
        }
    }
    return completed;
}
