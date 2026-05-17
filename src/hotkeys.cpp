#include "reach/hotkeys.h"

#include <windows.h>

#include <new>
#include <vector>

struct reach_hotkeys {
    std::vector<int> ids;
    std::vector<reach_hotkey_command> commands;
};

reach_result reach_hotkeys_create(reach_hotkeys **out_hotkeys)
{
    if (out_hotkeys == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_hotkeys *hotkeys = new (std::nothrow) reach_hotkeys();
    if (hotkeys == nullptr) {
        *out_hotkeys = nullptr;
        return REACH_ERROR;
    }

    *out_hotkeys = hotkeys;
    return REACH_OK;
}

void reach_hotkeys_destroy(reach_hotkeys *hotkeys)
{
    if (hotkeys != nullptr) {
        (void)reach_hotkeys_unregister_all(hotkeys);
    }
    delete hotkeys;
}

reach_result reach_hotkeys_register(reach_hotkeys *hotkeys, const reach_hotkey_config *config, uint32_t count)
{
    if (hotkeys == nullptr || (config == nullptr && count != 0)) {
        return REACH_INVALID_ARGUMENT;
    }

    (void)reach_hotkeys_unregister_all(hotkeys);

    for (uint32_t index = 0; index < count; ++index) {
        int id = static_cast<int>(index + 1);
        if (!RegisterHotKey(nullptr, id, config[index].modifiers, config[index].key)) {
            (void)reach_hotkeys_unregister_all(hotkeys);
            return REACH_ERROR;
        }

        hotkeys->ids.push_back(id);
        hotkeys->commands.push_back(static_cast<reach_hotkey_command>(config[index].command));
    }

    return REACH_OK;
}

reach_result reach_hotkeys_unregister_all(reach_hotkeys *hotkeys)
{
    if (hotkeys == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    for (int id : hotkeys->ids) {
        UnregisterHotKey(nullptr, id);
    }

    hotkeys->ids.clear();
    hotkeys->commands.clear();
    return REACH_OK;
}

reach_hotkey_command reach_hotkeys_translate(uint32_t id)
{
    switch (id) {
    case 1:
        return REACH_HOTKEY_SNAP_LEFT;
    case 2:
        return REACH_HOTKEY_SNAP_RIGHT;
    case 3:
        return REACH_HOTKEY_SNAP_TOP;
    case 4:
        return REACH_HOTKEY_SNAP_BOTTOM;
    case 5:
        return REACH_HOTKEY_SNAP_FULL;
    case 6:
        return REACH_HOTKEY_SEARCH;
    default:
        return REACH_HOTKEY_NONE;
    }
}
