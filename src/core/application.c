#include "reach/core/application.h"

static uint16_t reach_application_ascii_lower(uint16_t value)
{
    return value >= 'A' && value <= 'Z' ? (uint16_t)(value + ('a' - 'A')) : value;
}

static int32_t reach_application_text_equal(const uint16_t *a, const uint16_t *b,
                                            int32_t path)
{
    if (a == NULL || b == NULL)
    {
        return a == b;
    }
    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        uint16_t left = reach_application_ascii_lower(a[index]);
        uint16_t right = reach_application_ascii_lower(b[index]);
        if (path)
        {
            left = left == '/' ? '\\' : left;
            right = right == '/' ? '\\' : right;
        }
        if (left != right)
        {
            return 0;
        }
        ++index;
    }
    return a[index] == b[index];
}

static void reach_application_copy_text(uint16_t *target, const uint16_t *source)
{
    size_t index = 0;
    while (source[index] != 0 && index + 1 < REACH_APPLICATION_TEXT_CAPACITY)
    {
        target[index] = source[index];
        ++index;
    }
    target[index] = 0;
}

int32_t reach_application_identity_matches(const reach_application_identity *a,
                                           const reach_application_identity *b)
{
    if (a == NULL || b == NULL)
    {
        return 0;
    }
    if (a->app_user_model_id[0] != 0 && b->app_user_model_id[0] != 0 &&
        reach_application_text_equal(a->app_user_model_id, b->app_user_model_id, 0))
    {
        return 1;
    }
    for (size_t left = 0; left < a->runtime_path_count; ++left)
    {
        if (a->runtime_paths[left][0] == 0)
        {
            continue;
        }
        for (size_t right = 0; right < b->runtime_path_count; ++right)
        {
            if (b->runtime_paths[right][0] != 0 &&
                reach_application_text_equal(a->runtime_paths[left], b->runtime_paths[right], 1))
            {
                return 1;
            }
        }
    }
    return 0;
}

int32_t reach_application_identity_add_runtime_path(reach_application_identity *identity,
                                                    const uint16_t *path)
{
    if (identity == NULL || path == NULL || path[0] == 0)
    {
        return 0;
    }
    for (size_t index = 0; index < identity->runtime_path_count; ++index)
    {
        if (reach_application_text_equal(identity->runtime_paths[index], path, 1))
        {
            return 0;
        }
    }
    if (identity->runtime_path_count >= REACH_APPLICATION_RUNTIME_PATH_CAPACITY)
    {
        return 0;
    }
    reach_application_copy_text(identity->runtime_paths[identity->runtime_path_count], path);
    identity->runtime_path_count += 1;
    return 1;
}

int32_t reach_application_identity_merge(reach_application_identity *target,
                                         const reach_application_identity *source)
{
    if (target == NULL || source == NULL)
    {
        return 0;
    }
    int32_t changed = 0;
    if (target->app_user_model_id[0] == 0 && source->app_user_model_id[0] != 0)
    {
        reach_application_copy_text(target->app_user_model_id, source->app_user_model_id);
        changed = 1;
    }
    for (size_t index = 0; index < source->runtime_path_count; ++index)
    {
        changed |= reach_application_identity_add_runtime_path(target,
                                                               source->runtime_paths[index]);
    }
    return changed;
}

const uint16_t *reach_application_identity_primary_runtime_path(
    const reach_application_identity *identity)
{
    return identity != NULL && identity->runtime_path_count > 0 ? identity->runtime_paths[0] : NULL;
}
