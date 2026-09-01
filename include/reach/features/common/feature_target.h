#ifndef REACH_FEATURES_COMMON_FEATURE_TARGET_H
#define REACH_FEATURES_COMMON_FEATURE_TARGET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_feature_target_kind
    {
        REACH_FEATURE_TARGET_NONE = 0,

        REACH_FEATURE_TARGET_APP,
        REACH_FEATURE_TARGET_PATH,
        REACH_FEATURE_TARGET_TERMINAL_COMMAND,

        REACH_FEATURE_TARGET_LOCATION,
        REACH_FEATURE_TARGET_SHELL_LOCATION,
        REACH_FEATURE_TARGET_DEFAULT_LOCATION
    } reach_feature_target_kind;

    typedef struct reach_feature_target
    {
        reach_feature_target_kind kind;
        const uint16_t *path;
        const uint16_t *arguments;
        const uint16_t *app_user_model_id;
        const uint16_t *icon_ref;
    } reach_feature_target;

#ifdef __cplusplus
}
#endif

#endif
