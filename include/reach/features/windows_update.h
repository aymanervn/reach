#ifndef REACH_FEATURES_WINDOWS_UPDATE_H
#define REACH_FEATURES_WINDOWS_UPDATE_H

#include "reach/core/windows_update.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t reach_windows_update_matches_security_maintenance(
    const reach_windows_update_item *update);
void reach_windows_update_apply_default_selection(reach_windows_update_list *updates);
const uint16_t *reach_windows_update_state_label(reach_windows_update_state state);
const uint16_t *reach_windows_update_failure_label(reach_windows_update_failure_class failure);

#ifdef __cplusplus
}
#endif
#endif
