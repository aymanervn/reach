#include "windows_adapters_internal.h"

#include <windows.h>

#include <new>
#include <vector>

struct reach_hotkeys {
    std::vector<int> ids;
    std::vector<reach_hotkey_command> commands;
};

static reach_result reach_hotkeys_unregister_all(reach_hotkeys *hotkeys);

static reach_result reach_hotkeys_create(reach_hotkeys **out_hotkeys)
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

static void reach_hotkeys_destroy(reach_hotkeys *hotkeys)
{
    if (hotkeys != nullptr) {
        (void)reach_hotkeys_unregister_all(hotkeys);
    }
    delete hotkeys;
}

static reach_result reach_hotkeys_register(reach_hotkeys *hotkeys, const reach_hotkey_config *config, uint32_t count)
{
    if (hotkeys == nullptr || (config == nullptr && count != 0)) {
        return REACH_INVALID_ARGUMENT;
    }

    (void)reach_hotkeys_unregister_all(hotkeys);
    hotkeys->ids.reserve(count);
    hotkeys->commands.reserve(count);

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

static reach_result reach_hotkeys_unregister_all(reach_hotkeys *hotkeys)
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

static reach_hotkey_command reach_hotkeys_translate_registered(const reach_hotkeys *hotkeys, uint32_t id)
{
    if (hotkeys == nullptr) {
        return REACH_HOTKEY_NONE;
    }

    for (size_t index = 0; index < hotkeys->ids.size(); ++index) {
        if (hotkeys->ids[index] == static_cast<int>(id)) {
            return hotkeys->commands[index];
        }
    }

    return REACH_HOTKEY_NONE;
}

reach_result reach_windows_create_hotkeys(reach_hotkeys_port *out_port)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_result result = reach_hotkeys_create(&out_port->hotkeys);
    if (result != REACH_OK) {
        return result;
    }

    out_port->ops.destroy = reach_hotkeys_destroy;
    out_port->ops.register_hotkeys = reach_hotkeys_register;
    out_port->ops.unregister_all = reach_hotkeys_unregister_all;
    out_port->ops.translate_registered = reach_hotkeys_translate_registered;
    return REACH_OK;
}
