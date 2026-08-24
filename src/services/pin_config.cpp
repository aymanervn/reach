#include "reach/services/pin_config.h"

static int32_t reach_pin_path_equals(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }
    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        uint16_t ca = a[index];
        uint16_t cb = b[index];
        if (ca >= 'A' && ca <= 'Z')
        {
            ca = (uint16_t)(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z')
        {
            cb = (uint16_t)(cb + ('a' - 'A'));
        }
        if (ca != cb)
        {
            return 0;
        }
        ++index;
    }
    return a[index] == b[index];
}

static int32_t reach_pin_id_used_before(const reach_config_snapshot *snapshot, size_t before_index,
                                        uint32_t id)
{
    if (snapshot == nullptr || id == 0)
    {
        return 0;
    }
    for (size_t index = 0; index < before_index; ++index)
    {
        if (snapshot->pinned_apps[index].id == id)
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_pin_id_used(const reach_config_snapshot *snapshot, uint32_t id)
{
    if (snapshot == nullptr || id == 0)
    {
        return 0;
    }
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        if (snapshot->pinned_apps[index].id == id)
        {
            return 1;
        }
    }
    return 0;
}

static uint32_t reach_pin_next_available_id(const reach_config_snapshot *snapshot)
{
    uint32_t id = 1;
    while (reach_pin_id_used(snapshot, id))
    {
        ++id;
    }
    return id;
}

static int32_t reach_pin_ensure_ids(reach_config_snapshot *snapshot)
{
    int32_t changed = 0;
    if (snapshot == nullptr)
    {
        return changed;
    }
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        if (snapshot->pinned_apps[index].id == 0 ||
            reach_pin_id_used_before(snapshot, index, snapshot->pinned_apps[index].id))
        {
            snapshot->pinned_apps[index].id = 0;
            snapshot->pinned_apps[index].id = reach_pin_next_available_id(snapshot);
            changed = 1;
        }
    }
    return changed;
}

static reach_result reach_pin_add_default_explorer(reach_config_snapshot *snapshot)
{
    if (snapshot == nullptr || snapshot->pinned_app_count >= REACH_MAX_PINNED_APPS)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_pinned_app_model *app = &snapshot->pinned_apps[snapshot->pinned_app_count];
    *app = {};
    app->id = reach_pin_next_available_id(snapshot);
    const uint16_t explorer_path[] = {'C', ':', '\\', 'W', 'i', 'n', 'd', 'o', 'w', 's', '\\', 'e',
                                      'x', 'p', 'l',  'o', 'r', 'e', 'r', '.', 'e', 'x', 'e',  0};
    (void)reach_copy_utf16(app->path, 260, explorer_path);
    (void)reach_copy_utf16(app->icon_ref, 260, explorer_path);
    snapshot->pinned_app_count += 1;
    return REACH_OK;
}

static void reach_pin_set_changed(int32_t *out_changed, int32_t changed)
{
    if (out_changed != nullptr)
    {
        *out_changed = changed;
    }
}

reach_result reach_pin_config_ensure_defaults(reach_config_snapshot *snapshot,
                                              int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    int32_t changed = reach_pin_ensure_ids(snapshot);
    if (snapshot->pinned_app_count == 0)
    {
        reach_result result = reach_pin_add_default_explorer(snapshot);
        if (result != REACH_OK)
        {
            return result;
        }
        changed = 1;
    }
    reach_pin_set_changed(out_changed, changed);
    return REACH_OK;
}

reach_result reach_pin_config_pin_path(reach_config_snapshot *snapshot, const uint16_t *path,
                                       int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    int32_t changed = 0;
    if (snapshot->pinned_app_count == 0)
    {
        reach_result result = reach_pin_add_default_explorer(snapshot);
        if (result != REACH_OK)
        {
            return result;
        }
        changed = 1;
    }
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        if (reach_pin_path_equals(snapshot->pinned_apps[index].path, path))
        {
            reach_pin_set_changed(out_changed, changed);
            return REACH_OK;
        }
    }
    if (snapshot->pinned_app_count >= REACH_MAX_PINNED_APPS)
    {
        return REACH_ERROR;
    }
    reach_pinned_app_model *app = &snapshot->pinned_apps[snapshot->pinned_app_count];
    *app = {};
    app->id = reach_pin_next_available_id(snapshot);
    (void)reach_copy_utf16(app->path, 260, path);
    (void)reach_copy_utf16(app->icon_ref, 260, path);
    snapshot->pinned_app_count += 1;
    reach_pin_set_changed(out_changed, 1);
    return REACH_OK;
}

reach_result reach_pin_config_pin_app(reach_config_snapshot *snapshot,
                                      const reach_pinned_app_model *app, int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr || app == nullptr || app->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    int32_t default_added = 0;
    if (snapshot->pinned_app_count == 0)
    {
        reach_result result = reach_pin_add_default_explorer(snapshot);
        if (result != REACH_OK)
        {
            return result;
        }
        default_added = 1;
    }
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        if (!reach_pin_path_equals(snapshot->pinned_apps[index].path, app->path))
        {
            continue;
        }
        int32_t changed = default_added;
        if (snapshot->pinned_apps[index].arguments[0] == 0 && app->arguments[0] != 0)
        {
            (void)reach_copy_utf16(snapshot->pinned_apps[index].arguments, 260, app->arguments);
            changed = 1;
        }
        if (snapshot->pinned_apps[index].app_user_model_id[0] == 0 &&
            app->app_user_model_id[0] != 0)
        {
            (void)reach_copy_utf16(snapshot->pinned_apps[index].app_user_model_id, 260,
                                   app->app_user_model_id);
            changed = 1;
        }
        reach_pin_set_changed(out_changed, changed);
        return REACH_OK;
    }
    if (snapshot->pinned_app_count >= REACH_MAX_PINNED_APPS)
    {
        return REACH_ERROR;
    }
    reach_pinned_app_model *pinned = &snapshot->pinned_apps[snapshot->pinned_app_count];
    *pinned = {};
    pinned->id = reach_pin_next_available_id(snapshot);
    (void)reach_copy_utf16(pinned->path, 260, app->path);
    (void)reach_copy_utf16(pinned->arguments, 260, app->arguments);
    (void)reach_copy_utf16(pinned->icon_ref, 260,
                           app->icon_ref[0] != 0 ? app->icon_ref : app->path);
    (void)reach_copy_utf16(pinned->app_user_model_id, 260, app->app_user_model_id);
    snapshot->pinned_app_count += 1;
    reach_pin_set_changed(out_changed, 1);
    return REACH_OK;
}

reach_result reach_pin_config_move_id(reach_config_snapshot *snapshot, uint32_t id,
                                      size_t target_index, int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr || id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (snapshot->pinned_app_count == 0)
    {
        return REACH_OK;
    }
    if (target_index >= snapshot->pinned_app_count)
    {
        target_index = snapshot->pinned_app_count - 1;
    }
    size_t source_index = snapshot->pinned_app_count;
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        if (snapshot->pinned_apps[index].id == id)
        {
            source_index = index;
            break;
        }
    }
    if (source_index == snapshot->pinned_app_count || source_index == target_index)
    {
        return REACH_OK;
    }
    reach_pinned_app_model moved = snapshot->pinned_apps[source_index];
    if (source_index < target_index)
    {
        for (size_t index = source_index; index < target_index; ++index)
        {
            snapshot->pinned_apps[index] = snapshot->pinned_apps[index + 1];
        }
    }
    else
    {
        for (size_t index = source_index; index > target_index; --index)
        {
            snapshot->pinned_apps[index] = snapshot->pinned_apps[index - 1];
        }
    }
    snapshot->pinned_apps[target_index] = moved;
    reach_pin_set_changed(out_changed, 1);
    return REACH_OK;
}

reach_result reach_pin_config_set_app_user_model_id(reach_config_snapshot *snapshot,
                                                    const uint16_t *path,
                                                    const uint16_t *app_user_model_id,
                                                    int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr || path == nullptr || path[0] == 0 ||
        app_user_model_id == nullptr || app_user_model_id[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        if (reach_pin_path_equals(snapshot->pinned_apps[index].path, path))
        {
            if (!reach_pin_path_equals(snapshot->pinned_apps[index].app_user_model_id,
                                       app_user_model_id))
            {
                (void)reach_copy_utf16(snapshot->pinned_apps[index].app_user_model_id, 260,
                                       app_user_model_id);
                reach_pin_set_changed(out_changed, 1);
            }
            return REACH_OK;
        }
    }
    return REACH_ERROR;
}

reach_result reach_pin_config_unpin_id(reach_config_snapshot *snapshot, uint32_t id,
                                       int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr || id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    size_t write_index = 0;
    for (size_t read_index = 0; read_index < snapshot->pinned_app_count; ++read_index)
    {
        if (snapshot->pinned_apps[read_index].id != id)
        {
            if (write_index != read_index)
            {
                snapshot->pinned_apps[write_index] = snapshot->pinned_apps[read_index];
            }
            ++write_index;
        }
    }
    if (write_index != snapshot->pinned_app_count)
    {
        snapshot->pinned_app_count = write_index;
        reach_pin_set_changed(out_changed, 1);
    }
    return REACH_OK;
}

reach_result reach_pin_config_unpin_path(reach_config_snapshot *snapshot, const uint16_t *path,
                                         int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    size_t write_index = 0;
    for (size_t read_index = 0; read_index < snapshot->pinned_app_count; ++read_index)
    {
        if (!reach_pin_path_equals(snapshot->pinned_apps[read_index].path, path))
        {
            if (write_index != read_index)
            {
                snapshot->pinned_apps[write_index] = snapshot->pinned_apps[read_index];
            }
            ++write_index;
        }
    }
    if (write_index != snapshot->pinned_app_count)
    {
        snapshot->pinned_app_count = write_index;
        reach_pin_set_changed(out_changed, 1);
    }
    return REACH_OK;
}
