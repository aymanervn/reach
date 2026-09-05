#ifndef REACH_CORE_APPLICATION_H
#define REACH_CORE_APPLICATION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_APPLICATION_TEXT_CAPACITY 260
#define REACH_APPLICATION_RUNTIME_PATH_CAPACITY 8

    typedef enum reach_application_launch_kind
    {
        REACH_APPLICATION_LAUNCH_NONE = 0,
        REACH_APPLICATION_LAUNCH_EXECUTABLE = 1,
        REACH_APPLICATION_LAUNCH_SHORTCUT = 2,
        REACH_APPLICATION_LAUNCH_SHELL = 3,
        REACH_APPLICATION_LAUNCH_PACKAGED = 4
    } reach_application_launch_kind;

    typedef struct reach_application_identity
    {
        uint16_t app_user_model_id[REACH_APPLICATION_TEXT_CAPACITY];
        uint16_t runtime_paths[REACH_APPLICATION_RUNTIME_PATH_CAPACITY]
                              [REACH_APPLICATION_TEXT_CAPACITY];
        size_t runtime_path_count;
    } reach_application_identity;

    typedef struct reach_application_launch_target
    {
        reach_application_launch_kind kind;
        uint16_t path[REACH_APPLICATION_TEXT_CAPACITY];
        uint16_t arguments[REACH_APPLICATION_TEXT_CAPACITY];
    } reach_application_launch_target;

    typedef struct reach_application
    {
        reach_application_identity identity;
        reach_application_launch_target launch;
        uint16_t icon_ref[REACH_APPLICATION_TEXT_CAPACITY];
    } reach_application;

    int32_t reach_application_identity_matches(const reach_application_identity *a,
                                               const reach_application_identity *b);
    int32_t reach_application_identity_same(const reach_application_identity *a,
                                            const reach_application_identity *b);
    int32_t reach_application_identity_add_runtime_path(reach_application_identity *identity,
                                                        const uint16_t *path);
    int32_t reach_application_identity_merge(reach_application_identity *target,
                                             const reach_application_identity *source);
    const uint16_t *reach_application_identity_primary_runtime_path(
        const reach_application_identity *identity);

#ifdef __cplusplus
}
#endif

#endif
