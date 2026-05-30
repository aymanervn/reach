#include "reach/features/pin_config.h"

static int32_t reach_pin_path_equals(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr) {
        return 0;
    }

    size_t index = 0;
    while (a[index] != 0 && b[index] != 0) {
        uint16_t ca = a[index];
        uint16_t cb = b[index];
        if (ca >= 'A' && ca <= 'Z') {
            ca = (uint16_t)(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (uint16_t)(cb + ('a' - 'A'));
        }
        if (ca != cb) {
            return 0;
        }
        ++index;
    }

    return a[index] == b[index];
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

    const uint16_t *name = path;
    for (const uint16_t *cursor = path; *cursor != 0; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') {
            name = cursor + 1;
        }
    }

    size_t name_length = 0;
    while (name[name_length] != 0) {
        ++name_length;
    }

    size_t end = name_length;
    for (size_t index = name_length; index > 0; --index) {
        if (name[index - 1] == '.') {
            end = index - 1;
            break;
        }
    }
    if (end == 0) {
        end = name_length;
    }

    size_t write = 0;
    while (write + 1 < title_count && write < end) {
        title[write] = name[write];
        ++write;
    }
    title[write] = 0;
}

static reach_result reach_pin_add_default_explorer(reach_config_snapshot *snapshot)
{
    if (snapshot == nullptr || snapshot->pinned_app_count >= REACH_MAX_PINNED_APPS) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_pinned_app_model *app = &snapshot->pinned_apps[snapshot->pinned_app_count];
    *app = {};

    app->id = (uint32_t)(snapshot->pinned_app_count + 1);

    const uint16_t explorer_title[] = {
        'e','x','p','l','o','r','e','r',0
    };

    const uint16_t explorer_path[] = {
        'C',':','\\','W','i','n','d','o','w','s','\\',
        'e','x','p','l','o','r','e','r','.','e','x','e',0
    };

    (void)reach_copy_utf16(app->title, 128, explorer_title);
    (void)reach_copy_utf16(app->path, 260, explorer_path);
    (void)reach_copy_utf16(app->icon_ref, 260, explorer_path);

    app->app_user_model_id[0] = 0;
    app->arguments[0] = 0;

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

reach_result reach_pin_config_pin_app(
    reach_config_store_port *store,
    const reach_pinned_app_model *app)
{
    if (app == nullptr || app->path[0] == 0) {
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
        if (reach_pin_path_equals(snapshot.pinned_apps[index].path, app->path)) {
            int32_t changed = 0;
            if (snapshot.pinned_apps[index].arguments[0] == 0 &&
                app->arguments[0] != 0) {
                (void)reach_copy_utf16(
                    snapshot.pinned_apps[index].arguments,
                    260,
                    app->arguments);
                changed = 1;
            }
            if (snapshot.pinned_apps[index].app_user_model_id[0] == 0 &&
                app->app_user_model_id[0] != 0) {
                (void)reach_copy_utf16(
                    snapshot.pinned_apps[index].app_user_model_id,
                    260,
                    app->app_user_model_id);
                changed = 1;
            }
            if (changed) {
                return reach_pin_save(store, &snapshot);
            }
            return REACH_OK;
        }
    }

    if (snapshot.pinned_app_count >= REACH_MAX_PINNED_APPS) {
        return REACH_ERROR;
    }

    reach_pinned_app_model *pinned = &snapshot.pinned_apps[snapshot.pinned_app_count];
    *pinned = {};
    pinned->id = (uint32_t)(snapshot.pinned_app_count + 1);

    if (app->title[0] != 0) {
        (void)reach_copy_utf16(pinned->title, 128, app->title);
    } else {
        reach_pin_title_from_path(pinned->title, 128, app->path);
    }

    (void)reach_copy_utf16(pinned->path, 260, app->path);
    (void)reach_copy_utf16(pinned->arguments, 260, app->arguments);

    if (app->icon_ref[0] != 0) {
        (void)reach_copy_utf16(pinned->icon_ref, 260, app->icon_ref);
    } else {
        (void)reach_copy_utf16(pinned->icon_ref, 260, app->path);
    }

    (void)reach_copy_utf16(
        pinned->app_user_model_id,
        260,
        app->app_user_model_id);

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

reach_result reach_pin_config_set_app_user_model_id(
    reach_config_store_port *store,
    const uint16_t *path,
    const uint16_t *app_user_model_id)
{
    if (path == nullptr || path[0] == 0 ||
        app_user_model_id == nullptr || app_user_model_id[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_config_snapshot snapshot = {};
    reach_result result = reach_pin_load(store, &snapshot);
    if (result != REACH_OK) {
        return result;
    }

    for (size_t index = 0; index < snapshot.pinned_app_count; ++index) {
        if (reach_pin_path_equals(snapshot.pinned_apps[index].path, path)) {
            (void)reach_copy_utf16(
                snapshot.pinned_apps[index].app_user_model_id,
                260,
                app_user_model_id);
            return reach_pin_save(store, &snapshot);
        }
    }

    return REACH_ERROR;
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
