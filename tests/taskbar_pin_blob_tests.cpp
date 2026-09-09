#include "taskbar_pin_blob.h"

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static int32_t count_entry(const uint8_t *pidl, size_t size, void *user)
{
    size_t *count = static_cast<size_t *>(user);
    *count += pidl != nullptr && size == 4 ? 1 : 0;
    return 1;
}

int main()
{
    int failed = 0;
    size_t count = 0;
    const uint8_t valid[] = {0, 4, 0, 0, 0, 2, 0, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 0xff};
    failed +=
        expect(reach_taskbar_pin_blob_visit(valid, sizeof(valid), count_entry, &count) == REACH_OK);
    failed += expect(count == 2);

    const uint8_t missing_marker[] = {1, 4, 0, 0, 0, 2, 0, 0, 0, 0xff};
    failed += expect(reach_taskbar_pin_blob_visit(missing_marker, sizeof(missing_marker),
                                                  count_entry, &count) == REACH_ERROR);

    const uint8_t truncated[] = {0, 5, 0, 0, 0, 2, 0, 0, 0, 0xff};
    failed += expect(reach_taskbar_pin_blob_visit(truncated, sizeof(truncated), count_entry,
                                                  &count) == REACH_ERROR);
    return failed == 0 ? 0 : 1;
}
