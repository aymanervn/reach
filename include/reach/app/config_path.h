#ifndef REACH_CONFIG_PATH_H
#define REACH_CONFIG_PATH_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C" {
#endif

reach_result reach_default_config_path(uint16_t *path, uint32_t path_count);

#ifdef __cplusplus
}
#endif

#endif
