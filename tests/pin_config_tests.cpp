#include "reach/app/pin_config.h"

struct test_store {
    reach_config_snapshot snapshot;
    int save_count;
};

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
{
    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0) {
        dst[index] = (uint16_t)src[index];
        ++index;
    }
    dst[index] = 0;
}

static reach_result test_load(reach_config_store *store, reach_config_snapshot *out_snapshot)
{
    if (store == nullptr || out_snapshot == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    *out_snapshot = reinterpret_cast<test_store *>(store)->snapshot;
    return REACH_OK;
}

static reach_result test_save(reach_config_store *store, const reach_config_snapshot *snapshot)
{
    if (store == nullptr || snapshot == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    test_store *actual = reinterpret_cast<test_store *>(store);
    actual->snapshot = *snapshot;
    actual->save_count += 1;
    return REACH_OK;
}

int main()
{
    int failed = 0;

    test_store backing = {};
    backing.snapshot.pinned_app_count = 4;
    for (size_t index = 0; index < backing.snapshot.pinned_app_count; ++index) {
        backing.snapshot.pinned_apps[index].id = (uint32_t)(index + 1);
    }
    copy_ascii(backing.snapshot.pinned_apps[0].path, 260, "a.exe");
    copy_ascii(backing.snapshot.pinned_apps[1].path, 260, "b.exe");
    copy_ascii(backing.snapshot.pinned_apps[2].path, 260, "c.exe");
    copy_ascii(backing.snapshot.pinned_apps[3].path, 260, "d.exe");

    reach_config_store_port port = {};
    port.store = reinterpret_cast<reach_config_store *>(&backing);
    port.ops.load = test_load;
    port.ops.save = test_save;

    failed += expect(reach_pin_config_move_id(&port, 2, 3) == REACH_OK);
    failed += expect(backing.save_count == 1);
    failed += expect(backing.snapshot.pinned_app_count == 4);
    failed += expect(backing.snapshot.pinned_apps[0].path[0] == 'a');
    failed += expect(backing.snapshot.pinned_apps[1].path[0] == 'c');
    failed += expect(backing.snapshot.pinned_apps[2].path[0] == 'd');
    failed += expect(backing.snapshot.pinned_apps[3].path[0] == 'b');
    failed += expect(backing.snapshot.pinned_apps[0].id == 1);
    failed += expect(backing.snapshot.pinned_apps[1].id == 2);
    failed += expect(backing.snapshot.pinned_apps[2].id == 3);
    failed += expect(backing.snapshot.pinned_apps[3].id == 4);

    failed += expect(reach_pin_config_move_id(&port, 99, 0) == REACH_OK);
    failed += expect(backing.save_count == 1);

    return failed == 0 ? 0 : 1;
}
