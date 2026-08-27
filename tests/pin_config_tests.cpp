#include "reach/support/util.h"
#include "reach/services/pin_config.h"

#include <stdio.h>

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main()
{
    int failed = 0;
    int32_t changed = 0;

    reach_config_snapshot snapshot = {};
    snapshot.pinned_app_count = 4;
    for (size_t index = 0; index < snapshot.pinned_app_count; ++index)
    {
        snapshot.pinned_apps[index].id = (uint32_t)(index + 1);
    }
    reach_copy_ascii_to_utf16(snapshot.pinned_apps[0].path, 260, "a.exe");
    reach_copy_ascii_to_utf16(snapshot.pinned_apps[1].path, 260, "b.exe");
    reach_copy_ascii_to_utf16(snapshot.pinned_apps[2].path, 260, "c.exe");
    reach_copy_ascii_to_utf16(snapshot.pinned_apps[3].path, 260, "d.exe");

    failed += expect(reach_pin_config_move_id(&snapshot, 2, 3, &changed) == REACH_OK);
    failed += expect(changed == 1);
    failed += expect(snapshot.pinned_app_count == 4);
    failed += expect(snapshot.pinned_apps[0].path[0] == 'a');
    failed += expect(snapshot.pinned_apps[1].path[0] == 'c');
    failed += expect(snapshot.pinned_apps[2].path[0] == 'd');
    failed += expect(snapshot.pinned_apps[3].path[0] == 'b');
    failed += expect(snapshot.pinned_apps[0].id == 1);
    failed += expect(snapshot.pinned_apps[1].id == 3);
    failed += expect(snapshot.pinned_apps[2].id == 4);
    failed += expect(snapshot.pinned_apps[3].id == 2);

    failed += expect(reach_pin_config_move_id(&snapshot, 99, 0, &changed) == REACH_OK);
    failed += expect(changed == 0);

    reach_pinned_app_model helper_app = {};
    reach_copy_ascii_to_utf16(helper_app.path, 260, "steam.exe");
    reach_copy_ascii_to_utf16(helper_app.arguments, 260, "-silent");
    reach_copy_ascii_to_utf16(helper_app.app_user_model_id, 260, "Valve.Steam.Client");

    failed += expect(reach_pin_config_pin_app(&snapshot, &helper_app, &changed) == REACH_OK);
    failed += expect(changed == 1);
    failed += expect(snapshot.pinned_app_count == 5);
    failed += expect(snapshot.pinned_apps[4].path[0] == 's');
    failed += expect(snapshot.pinned_apps[4].arguments[0] == '-');
    failed += expect(snapshot.pinned_apps[4].app_user_model_id[0] == 'V');

    reach_config_snapshot update_snapshot = {};
    update_snapshot.pinned_app_count = 1;
    update_snapshot.pinned_apps[0].id = 1;
    reach_copy_ascii_to_utf16(update_snapshot.pinned_apps[0].path, 260, "C:\\Apps\\helper.exe");

    reach_pinned_app_model update_app = {};
    reach_copy_ascii_to_utf16(update_app.path, 260, "c:/apps/HELPER.EXE");
    reach_copy_ascii_to_utf16(update_app.arguments, 260, "--app");
    reach_copy_ascii_to_utf16(update_app.app_user_model_id, 260, "Example.App");

    failed += expect(reach_pin_config_pin_app(&update_snapshot, &update_app, &changed) == REACH_OK);
    failed += expect(changed == 1);
    failed += expect(update_snapshot.pinned_app_count == 1);
    failed += expect(update_snapshot.pinned_apps[0].arguments[0] == '-');
    failed += expect(update_snapshot.pinned_apps[0].app_user_model_id[0] == 'E');

    uint16_t update_path[260] = {};
    uint16_t update_aumid[260] = {};
    reach_copy_ascii_to_utf16(update_path, 260, "C:/APPS/helper.exe");
    reach_copy_ascii_to_utf16(update_aumid, 260, "example.app");
    failed += expect(reach_pin_config_set_app_user_model_id(&update_snapshot, update_path,
                                                            update_aumid, &changed) == REACH_OK);
    failed += expect(changed == 0);

    reach_config_snapshot capacity_snapshot = {};
    capacity_snapshot.pinned_app_count = REACH_MAX_PINNED_APPS - 1;
    for (size_t index = 0; index < capacity_snapshot.pinned_app_count; ++index)
    {
        capacity_snapshot.pinned_apps[index].id = (uint32_t)(index + 1);
        char path[32] = {};
        snprintf(path, sizeof(path), "pin_%zu.exe", index);
        reach_copy_ascii_to_utf16(capacity_snapshot.pinned_apps[index].path, 260, path);
    }

    reach_pinned_app_model last_app = {};
    reach_copy_ascii_to_utf16(last_app.path, 260, "last.exe");
    failed += expect(reach_pin_config_pin_app(&capacity_snapshot, &last_app, &changed) == REACH_OK);
    failed += expect(changed == 1);
    failed += expect(capacity_snapshot.pinned_app_count == REACH_MAX_PINNED_APPS);

    reach_pinned_app_model overflow_app = {};
    reach_copy_ascii_to_utf16(overflow_app.path, 260, "overflow.exe");
    failed += expect(reach_pin_config_pin_app(&capacity_snapshot, &overflow_app, &changed) ==
                     REACH_ERROR);
    failed += expect(changed == 0);
    failed += expect(capacity_snapshot.pinned_app_count == REACH_MAX_PINNED_APPS);

    reach_config_snapshot defaults = {};
    failed += expect(reach_pin_config_ensure_defaults(&defaults, &changed) == REACH_OK);
    failed += expect(changed == 1);
    failed += expect(defaults.pinned_app_count == 1);
    failed += expect(defaults.pinned_apps[0].id == 1);

    return failed == 0 ? 0 : 1;
}
