#include "taskbar_pin_blob.h"

static int32_t reach_taskbar_pin_blob_pidl_valid(const uint8_t *data, size_t size)
{
    size_t cursor = 0;
    while (cursor + sizeof(uint16_t) <= size)
    {
        uint16_t item_size =
            (uint16_t)(data[cursor] | ((uint16_t)data[cursor + 1] << 8));
        if (item_size == 0)
        {
            return cursor + sizeof(uint16_t) == size;
        }
        if (item_size < sizeof(uint16_t) || cursor + item_size > size)
        {
            return 0;
        }
        cursor += item_size;
    }
    return 0;
}

reach_result reach_taskbar_pin_blob_visit(const uint8_t *data, size_t size,
                                          reach_taskbar_pin_blob_visitor visitor, void *user)
{
    if (data == nullptr || size == 0 || visitor == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t cursor = 0;
    while (cursor < size)
    {
        if (data[cursor] == 0xff && cursor + 1 == size)
        {
            return REACH_OK;
        }
        if (data[cursor] != 0)
        {
            return REACH_ERROR;
        }
        cursor += 1;
        if (cursor + sizeof(uint32_t) > size)
        {
            return REACH_ERROR;
        }
        uint32_t entry_size = (uint32_t)data[cursor] | ((uint32_t)data[cursor + 1] << 8) |
                              ((uint32_t)data[cursor + 2] << 16) |
                              ((uint32_t)data[cursor + 3] << 24);
        cursor += sizeof(uint32_t);
        if (entry_size < sizeof(uint16_t) || cursor + entry_size > size ||
            !reach_taskbar_pin_blob_pidl_valid(data + cursor, entry_size))
        {
            return REACH_ERROR;
        }
        if (!visitor(data + cursor, entry_size, user))
        {
            return REACH_ERROR;
        }
        cursor += entry_size;
    }
    return REACH_ERROR;
}
