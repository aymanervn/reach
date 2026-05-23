#include "reach/app/pin_config.h"

#include <windows.h>
#include <shlwapi.h>

static int32_t reach_pin_path_equals(const uint16_t *a, const uint16_t *b)
{
    return a != nullptr && b != nullptr &&
        CompareStringOrdinal(reinterpret_cast<const wchar_t *>(a), -1, reinterpret_cast<const wchar_t *>(b), -1, TRUE) == CSTR_EQUAL;
}

static void reach_pin_assign_ids(reach_config_snapshot *snapshot)
{
    if (snapshot == nullptr) {
        return;
    }
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index) {
        snapshot->pinned_apps[index].id = (uint32_t)(index + 1);
    }
}

static reach_result reach_pin_load(reach_config_store_port *store, reach_config_snapshot *snapshot)
{
    if (store == nullptr || store->ops.load == nullptr || snapshot == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    return store->ops.load(store->store, snapshot);
}

static reach_result reach_pin_save(reach_config_store_port *store, reach_config_snapshot *snapshot)
{
    if (store == nullptr || store->ops.save == nullptr || snapshot == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    reach_pin_assign_ids(snapshot);
    return store->ops.save(store->store, snapshot);
}

static void reach_pin_title_from_path(uint16_t *title, size_t title_count, const uint16_t *path)
{
    if (title == nullptr || title_count == 0) {
        return;
    }
    title[0] = 0;
    if (path == nullptr || path[0] == 0) {
        return;
    }

    const wchar_t *path_w = reinterpret_cast<const wchar_t *>(path);
    const wchar_t *name = PathFindFileNameW(path_w);
    if (name == nullptr || name[0] == 0) {
        name = path_w;
    }

    wchar_t clean[128] = {};
    wcscpy_s(clean, name);
    PathRemoveExtensionW(clean);
    if (clean[0] == 0) {
        wcscpy_s(clean, name);
    }
    (void)reach_copy_utf16(title, title_count, reinterpret_cast<const uint16_t *>(clean));
}

static reach_result reach_pin_add_default_explorer(reach_config_snapshot *snapshot)
{
    if (snapshot == nullptr || snapshot->pinned_app_count >= REACH_MAX_PINNED_APPS) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_pinned_app_model *app = &snapshot->pinned_apps[snapshot->pinned_app_count];
    *app = {};
    app->id = (uint32_t)(snapshot->pinned_app_count + 1);
    (void)reach_copy_utf16(app->title, 128, reinterpret_cast<const uint16_t *>(L"Explorer"));
    (void)reach_copy_utf16(app->path, 260, reinterpret_cast<const uint16_t *>(L"explorer.exe"));
    (void)reach_copy_utf16(app->icon_ref, 260, reinterpret_cast<const uint16_t *>(L"explorer.exe"));
    snapshot->pinned_app_count += 1;
    return REACH_OK;
}

reach_result reach_pin_config_ensure_defaults(reach_config_store_port *store)
{
    reach_config_snapshot snapshot = {};
    reach_result result = reach_pin_load(store, &snapshot);
    if (result != REACH_OK) {
        return result;
    }
    if (snapshot.pinned_app_count != 0) {
        return REACH_OK;
    }
    result = reach_pin_add_default_explorer(&snapshot);
    return result == REACH_OK ? reach_pin_save(store, &snapshot) : result;
}

reach_result reach_pin_config_pin_path(reach_config_store_port *store, const uint16_t *path)
{
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_config_snapshot snapshot = {};
    reach_result result = reach_pin_load(store, &snapshot);
    if (result != REACH_OK) {
        return result;
    }
    if (snapshot.pinned_app_count == 0) {
        result = reach_pin_add_default_explorer(&snapshot);
        if (result != REACH_OK) {
            return result;
        }
    }

    for (size_t index = 0; index < snapshot.pinned_app_count; ++index) {
        if (reach_pin_path_equals(snapshot.pinned_apps[index].path, path)) {
            return REACH_OK;
        }
    }
    if (snapshot.pinned_app_count >= REACH_MAX_PINNED_APPS) {
        return REACH_ERROR;
    }

    reach_pinned_app_model *app = &snapshot.pinned_apps[snapshot.pinned_app_count];
    *app = {};
    app->id = (uint32_t)(snapshot.pinned_app_count + 1);
    reach_pin_title_from_path(app->title, 128, path);
    (void)reach_copy_utf16(app->path, 260, path);
    (void)reach_copy_utf16(app->icon_ref, 260, path);
    snapshot.pinned_app_count += 1;
    return reach_pin_save(store, &snapshot);
}

reach_result reach_pin_config_move_id(reach_config_store_port *store, uint32_t id, size_t target_index)
{
    reach_config_snapshot snapshot = {};
    reach_result result = reach_pin_load(store, &snapshot);
    if (result != REACH_OK) {
        return result;
    }
    if (snapshot.pinned_app_count == 0) {
        return REACH_OK;
    }
    if (target_index >= snapshot.pinned_app_count) {
        target_index = snapshot.pinned_app_count - 1;
    }

    size_t source_index = snapshot.pinned_app_count;
    for (size_t index = 0; index < snapshot.pinned_app_count; ++index) {
        if (snapshot.pinned_apps[index].id == id) {
            source_index = index;
            break;
        }
    }
    if (source_index == snapshot.pinned_app_count || source_index == target_index) {
        return REACH_OK;
    }

    reach_pinned_app_model moved = snapshot.pinned_apps[source_index];
    if (source_index < target_index) {
        for (size_t index = source_index; index < target_index; ++index) {
            snapshot.pinned_apps[index] = snapshot.pinned_apps[index + 1];
        }
    } else {
        for (size_t index = source_index; index > target_index; --index) {
            snapshot.pinned_apps[index] = snapshot.pinned_apps[index - 1];
        }
    }
    snapshot.pinned_apps[target_index] = moved;
    return reach_pin_save(store, &snapshot);
}

reach_result reach_pin_config_unpin_id(reach_config_store_port *store, uint32_t id)
{
    reach_config_snapshot snapshot = {};
    reach_result result = reach_pin_load(store, &snapshot);
    if (result != REACH_OK) {
        return result;
    }

    size_t write_index = 0;
    for (size_t read_index = 0; read_index < snapshot.pinned_app_count; ++read_index) {
        if (snapshot.pinned_apps[read_index].id != id) {
            if (write_index != read_index) {
                snapshot.pinned_apps[write_index] = snapshot.pinned_apps[read_index];
            }
            ++write_index;
        }
    }
    if (write_index == snapshot.pinned_app_count) {
        return REACH_OK;
    }
    snapshot.pinned_app_count = write_index;
    return reach_pin_save(store, &snapshot);
}

reach_result reach_pin_config_unpin_path(reach_config_store_port *store, const uint16_t *path)
{
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_config_snapshot snapshot = {};
    reach_result result = reach_pin_load(store, &snapshot);
    if (result != REACH_OK) {
        return result;
    }

    size_t write_index = 0;
    for (size_t read_index = 0; read_index < snapshot.pinned_app_count; ++read_index) {
        if (!reach_pin_path_equals(snapshot.pinned_apps[read_index].path, path)) {
            if (write_index != read_index) {
                snapshot.pinned_apps[write_index] = snapshot.pinned_apps[read_index];
            }
            ++write_index;
        }
    }
    if (write_index == snapshot.pinned_app_count) {
        return REACH_OK;
    }
    snapshot.pinned_app_count = write_index;
    return reach_pin_save(store, &snapshot);
}
