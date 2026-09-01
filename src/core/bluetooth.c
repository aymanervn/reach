#include "reach/core/bluetooth.h"

static int reach_bluetooth_text_compare(const uint16_t *left, const uint16_t *right)
{
    size_t index = 0;
    while (left[index] != 0 && left[index] == right[index])
    {
        ++index;
    }
    if (left[index] == right[index])
    {
        return 0;
    }
    return left[index] < right[index] ? -1 : 1;
}

int32_t reach_bluetooth_device_id_equal(const uint16_t *left, const uint16_t *right)
{
    if (left == NULL || right == NULL)
    {
        return 0;
    }
    return reach_bluetooth_text_compare(left, right) == 0;
}

const uint16_t *reach_bluetooth_device_kind_label(reach_bluetooth_device_kind kind)
{
    switch (kind)
    {
    case REACH_BLUETOOTH_DEVICE_AUDIO:
        return (const uint16_t *)u"Audio";
    case REACH_BLUETOOTH_DEVICE_KEYBOARD:
        return (const uint16_t *)u"Keyboard";
    case REACH_BLUETOOTH_DEVICE_MOUSE:
        return (const uint16_t *)u"Mouse";
    case REACH_BLUETOOTH_DEVICE_PHONE:
        return (const uint16_t *)u"Phone";
    case REACH_BLUETOOTH_DEVICE_COMPUTER:
        return (const uint16_t *)u"Computer";
    case REACH_BLUETOOTH_DEVICE_WEARABLE:
        return (const uint16_t *)u"Wearable";
    case REACH_BLUETOOTH_DEVICE_PRINTER:
        return (const uint16_t *)u"Printer";
    default:
        return (const uint16_t *)u"Device";
    }
}

static void reach_bluetooth_device_merge(reach_bluetooth_device *kept,
                                         const reach_bluetooth_device *other)
{
    kept->paired = kept->paired || other->paired;
    kept->connected = kept->connected || other->connected;
    kept->can_pair = kept->can_pair || other->can_pair;
    if (kept->kind == REACH_BLUETOOTH_DEVICE_UNKNOWN)
    {
        kept->kind = other->kind;
    }
    if (kept->icon_path[0] == 0)
    {
        for (size_t index = 0; index < REACH_BLUETOOTH_ICON_PATH_CAPACITY; ++index)
        {
            kept->icon_path[index] = other->icon_path[index];
            if (other->icon_path[index] == 0)
            {
                break;
            }
        }
    }
}

static int reach_bluetooth_device_order(const reach_bluetooth_device *left,
                                        const reach_bluetooth_device *right)
{
    if (left->paired != right->paired)
    {
        return left->paired ? -1 : 1;
    }
    if (left->connected != right->connected)
    {
        return left->connected ? -1 : 1;
    }
    return reach_bluetooth_text_compare(left->name, right->name);
}

void reach_bluetooth_device_list_normalize(reach_bluetooth_device_list *list)
{
    if (list == NULL)
    {
        return;
    }
    if (list->count > REACH_BLUETOOTH_MAX_DEVICES)
    {
        list->count = REACH_BLUETOOTH_MAX_DEVICES;
    }

    size_t kept_count = 0;
    for (size_t index = 0; index < list->count; ++index)
    {
        const reach_bluetooth_device *candidate = &list->devices[index];
        if (candidate->id[0] == 0 || candidate->name[0] == 0)
        {
            continue;
        }

        int32_t merged = 0;
        for (size_t existing = 0; existing < kept_count; ++existing)
        {
            if (!reach_bluetooth_device_id_equal(list->devices[existing].id, candidate->id))
            {
                continue;
            }
            reach_bluetooth_device_merge(&list->devices[existing], candidate);
            merged = 1;
            break;
        }
        if (!merged)
        {
            list->devices[kept_count++] = *candidate;
        }
    }
    list->count = kept_count;

    for (size_t index = 1; index < list->count; ++index)
    {
        reach_bluetooth_device moving = list->devices[index];
        size_t position = index;
        while (position > 0 &&
               reach_bluetooth_device_order(&moving, &list->devices[position - 1]) < 0)
        {
            list->devices[position] = list->devices[position - 1];
            --position;
        }
        list->devices[position] = moving;
    }
}

size_t reach_bluetooth_device_list_find(const reach_bluetooth_device_list *list,
                                        const uint16_t *device_id)
{
    if (list == NULL || device_id == NULL || device_id[0] == 0)
    {
        return REACH_BLUETOOTH_MAX_DEVICES;
    }
    for (size_t index = 0; index < list->count; ++index)
    {
        if (reach_bluetooth_device_id_equal(list->devices[index].id, device_id))
        {
            return index;
        }
    }
    return REACH_BLUETOOTH_MAX_DEVICES;
}

size_t reach_bluetooth_paired_count(const reach_bluetooth_device_list *list)
{
    size_t count = 0;
    if (list == NULL)
    {
        return 0;
    }
    for (size_t index = 0; index < list->count; ++index)
    {
        if (list->devices[index].paired)
        {
            ++count;
        }
    }
    return count;
}
