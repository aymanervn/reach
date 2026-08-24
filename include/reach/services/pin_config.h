#ifndef REACH_SERVICES_PIN_CONFIG_H
#define REACH_SERVICES_PIN_CONFIG_H

#include "reach/core/config.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    reach_result reach_pin_config_ensure_defaults(reach_config_snapshot *snapshot,
                                                  int32_t *out_changed);
    reach_result reach_pin_config_pin_path(reach_config_snapshot *snapshot, const uint16_t *path,
                                           int32_t *out_changed);
    reach_result reach_pin_config_move_id(reach_config_snapshot *snapshot, uint32_t id,
                                          size_t target_index, int32_t *out_changed);
    reach_result reach_pin_config_unpin_id(reach_config_snapshot *snapshot, uint32_t id,
                                           int32_t *out_changed);
    reach_result reach_pin_config_unpin_path(reach_config_snapshot *snapshot,
                                             const uint16_t *path, int32_t *out_changed);
    reach_result reach_pin_config_set_app_user_model_id(reach_config_snapshot *snapshot,
                                                        const uint16_t *path,
                                                        const uint16_t *app_user_model_id,
                                                        int32_t *out_changed);
    reach_result reach_pin_config_pin_app(reach_config_snapshot *snapshot,
                                          const reach_pinned_app_model *app,
                                          int32_t *out_changed);

#ifdef __cplusplus
}
#endif

#endif
