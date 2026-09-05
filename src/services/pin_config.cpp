#include "reach/services/pin_config.h"

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

static void reach_pin_set_changed(int32_t *out_changed, int32_t changed)
{
    if (out_changed != nullptr)
    {
        *out_changed = changed;
    }
}

static int32_t reach_pin_merge_missing(reach_pinned_app_model *target,
                                       const reach_pinned_app_model *source)
{
    int32_t changed = 0;
    reach_application *target_application = &target->application;
    const reach_application *source_application = &source->application;
    size_t runtime_count = target_application->identity.runtime_path_count;
    int32_t had_app_user_model_id =
        target_application->identity.app_user_model_id[0] != 0;
    if (target_application->launch.kind == REACH_APPLICATION_LAUNCH_NONE &&
        source_application->launch.kind != REACH_APPLICATION_LAUNCH_NONE)
    {
        target_application->launch.kind = source_application->launch.kind;
        changed = 1;
    }
    if (target_application->launch.path[0] == 0 &&
        source_application->launch.path[0] != 0)
    {
        (void)reach_copy_utf16(target_application->launch.path, 260,
                               source_application->launch.path);
        changed = 1;
    }
    if (target_application->launch.arguments[0] == 0 &&
        source_application->launch.arguments[0] != 0)
    {
        (void)reach_copy_utf16(target_application->launch.arguments, 260,
                               source_application->launch.arguments);
        changed = 1;
    }
    if (target_application->icon_ref[0] == 0 && source_application->icon_ref[0] != 0)
    {
        (void)reach_copy_utf16(target_application->icon_ref, 260,
                               source_application->icon_ref);
        changed = 1;
    }
    reach_application_identity_merge(&target_application->identity,
                                     &source_application->identity);
    changed |= target_application->identity.runtime_path_count != runtime_count;
    changed |= !had_app_user_model_id &&
               target_application->identity.app_user_model_id[0] != 0;
    return changed;
}

static int32_t reach_pin_merge_duplicates(reach_config_snapshot *snapshot)
{
    int32_t changed = 0;
    size_t write = 0;
    for (size_t read = 0; read < snapshot->pinned_app_count; ++read)
    {
        size_t match = write;
        for (size_t index = 0; index < write; ++index)
        {
            if (reach_application_identity_matches(
                    &snapshot->pinned_apps[index].application.identity,
                    &snapshot->pinned_apps[read].application.identity))
            {
                match = index;
                break;
            }
        }
        if (match < write)
        {
            changed |=
                reach_pin_merge_missing(&snapshot->pinned_apps[match],
                                        &snapshot->pinned_apps[read]);
            changed = 1;
            continue;
        }
        if (write != read)
        {
            snapshot->pinned_apps[write] = snapshot->pinned_apps[read];
        }
        ++write;
    }
    for (size_t index = write; index < snapshot->pinned_app_count; ++index)
    {
        snapshot->pinned_apps[index] = {};
    }
    snapshot->pinned_app_count = write;
    return changed;
}

reach_result reach_pin_config_ensure_defaults(reach_config_snapshot *snapshot, int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    int32_t changed = reach_pin_merge_duplicates(snapshot);
    changed |= reach_pin_ensure_ids(snapshot);
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
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        if (reach_path_equals(snapshot->pinned_apps[index].application.launch.path, path))
        {
            reach_pin_set_changed(out_changed, 0);
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
    app->application.launch.kind = REACH_APPLICATION_LAUNCH_EXECUTABLE;
    (void)reach_copy_utf16(app->application.launch.path, 260, path);
    (void)reach_copy_utf16(app->application.icon_ref, 260, path);
    (void)reach_application_identity_add_runtime_path(&app->application.identity, path);
    snapshot->pinned_app_count += 1;
    reach_pin_set_changed(out_changed, 1);
    return REACH_OK;
}

reach_result reach_pin_config_pin_app(reach_config_snapshot *snapshot,
                                      const reach_pinned_app_model *app, int32_t *out_changed)
{
    reach_pin_set_changed(out_changed, 0);
    if (snapshot == nullptr || app == nullptr ||
        app->application.launch.path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        if (!reach_application_identity_matches(
                &snapshot->pinned_apps[index].application.identity,
                &app->application.identity))
        {
            continue;
        }
        int32_t changed = reach_pin_merge_missing(&snapshot->pinned_apps[index], app);
        reach_pin_set_changed(out_changed, changed);
        return REACH_OK;
    }
    if (snapshot->pinned_app_count >= REACH_MAX_PINNED_APPS)
    {
        return REACH_ERROR;
    }
    reach_pinned_app_model *pinned = &snapshot->pinned_apps[snapshot->pinned_app_count];
    *pinned = *app;
    pinned->id = reach_pin_next_available_id(snapshot);
    if (pinned->application.icon_ref[0] == 0)
    {
        (void)reach_copy_utf16(pinned->application.icon_ref, 260,
                               pinned->application.launch.path);
    }
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
    if (snapshot == nullptr || path == nullptr || path[0] == 0 || app_user_model_id == nullptr ||
        app_user_model_id[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    for (size_t index = 0; index < snapshot->pinned_app_count; ++index)
    {
        reach_application_identity *identity =
            &snapshot->pinned_apps[index].application.identity;
        for (size_t runtime_index = 0; runtime_index < identity->runtime_path_count;
             ++runtime_index)
        {
            if (!reach_path_equals(identity->runtime_paths[runtime_index], path))
            {
                continue;
            }
            if (!reach_utf16_equal_ascii_case_insensitive(identity->app_user_model_id,
                                                          app_user_model_id))
            {
                (void)reach_copy_utf16(identity->app_user_model_id, 260, app_user_model_id);
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
        const reach_pinned_app_model *app = &snapshot->pinned_apps[read_index];
        int32_t matches = reach_path_equals(app->application.launch.path, path);
        for (size_t runtime_index = 0;
             !matches && runtime_index < app->application.identity.runtime_path_count;
             ++runtime_index)
        {
            matches = reach_path_equals(
                app->application.identity.runtime_paths[runtime_index], path);
        }
        if (!matches)
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
