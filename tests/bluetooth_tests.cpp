#include "reach/support/util.h"
#include "reach/core/bluetooth.h"

#include <stdio.h>

static int failures = 0;

static void expect_true(int value, const char *message)
{
    if (!value)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static reach_bluetooth_device make_device(const char *id, const char *name, int32_t paired,
                                          int32_t connected)
{
    reach_bluetooth_device device = {};
    reach_copy_ascii_to_utf16(device.id, REACH_BLUETOOTH_DEVICE_ID_CAPACITY, id);
    reach_copy_ascii_to_utf16(device.name, REACH_BLUETOOTH_NAME_CAPACITY, name);
    device.paired = paired;
    device.connected = connected;
    return device;
}

/* The classic and LE watchers both report a dual-mode device, so the same association
   endpoint id arrives twice and must collapse into one row. */
static void test_normalize_merges_duplicate_ids(void)
{
    reach_bluetooth_device_list list = {};
    list.devices[list.count] = make_device("bt#aa", "Headphones", 0, 0);
    list.devices[list.count].kind = REACH_BLUETOOTH_DEVICE_UNKNOWN;
    ++list.count;
    list.devices[list.count] = make_device("bt#aa", "Headphones", 1, 1);
    list.devices[list.count].kind = REACH_BLUETOOTH_DEVICE_AUDIO;
    reach_copy_ascii_to_utf16(list.devices[list.count].icon_path,
                              REACH_BLUETOOTH_ICON_PATH_CAPACITY, "C:\\icons.dll,-3");
    ++list.count;

    reach_bluetooth_device_list_normalize(&list);

    expect_true(list.count == 1, "the same device id collapses into one row");
    expect_true(list.devices[0].paired == 1, "the merged row keeps the paired flag");
    expect_true(list.devices[0].connected == 1, "the merged row keeps the connected flag");
    expect_true(list.devices[0].kind == REACH_BLUETOOTH_DEVICE_AUDIO,
                "the merged row keeps the known device kind");
    expect_true(list.devices[0].icon_path[0] != 0, "the merged row keeps the Windows icon path");
}

static void test_normalize_drops_unnamed_devices(void)
{
    reach_bluetooth_device_list list = {};
    list.devices[list.count++] = make_device("bt#a", "", 0, 0);
    list.devices[list.count++] = make_device("", "Ghost", 0, 0);
    list.devices[list.count++] = make_device("bt#b", "Keyboard", 0, 0);

    reach_bluetooth_device_list_normalize(&list);

    expect_true(list.count == 1, "unnamed and id-less devices are dropped");
}

static void test_normalize_orders_paired_then_connected_then_name(void)
{
    reach_bluetooth_device_list list = {};
    list.devices[list.count++] = make_device("bt#d", "Zeta", 0, 0);
    list.devices[list.count++] = make_device("bt#c", "Alpha", 0, 0);
    list.devices[list.count++] = make_device("bt#b", "Speaker", 1, 0);
    list.devices[list.count++] = make_device("bt#a", "Mouse", 1, 1);

    reach_bluetooth_device_list_normalize(&list);

    expect_true(list.count == 4, "every distinct device survives");
    expect_true(list.devices[0].paired && list.devices[0].connected,
                "a paired connected device sorts first");
    expect_true(list.devices[1].paired, "paired devices sort above unpaired ones");
    expect_true(list.devices[2].name[0] == (uint16_t)'A', "unpaired devices sort by name");
    expect_true(reach_bluetooth_paired_count(&list) == 2, "paired devices are counted");

    size_t found = reach_bluetooth_device_list_find(&list, list.devices[3].id);
    expect_true(found == 3, "find locates a device by id after sorting");
}

int main(void)
{
    test_normalize_merges_duplicate_ids();
    test_normalize_drops_unnamed_devices();
    test_normalize_orders_paired_then_connected_then_name();

    if (failures == 0)
    {
        printf("bluetooth tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
