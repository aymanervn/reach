#ifndef REACH_PIN_CONFIG_H
#define REACH_PIN_CONFIG_H

#include "reach/ports/config_store.h"

#ifdef __cplusplus
extern "C" {
#endif

reach_result reach_pin_config_ensure_defaults(reach_config_store_port *store);
reach_result reach_pin_config_pin_path(reach_config_store_port *store, const uint16_t *path);
reach_result reach_pin_config_move_id(reach_config_store_port *store, uint32_t id, size_t target_index);
reach_result reach_pin_config_unpin_id(reach_config_store_port *store, uint32_t id);
reach_result reach_pin_config_unpin_path(reach_config_store_port *store, const uint16_t *path);

#ifdef __cplusplus
}
#endif

#endif
