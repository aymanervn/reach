#ifndef REACH_SHELL_H
#define REACH_SHELL_H

#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_shell reach_shell;

typedef struct reach_shell_desc {
    const uint16_t *config_path;
} reach_shell_desc;

reach_result reach_shell_create(const reach_shell_desc *desc, reach_shell **out_shell);
void reach_shell_destroy(reach_shell *shell);
reach_result reach_shell_start(reach_shell *shell);
reach_result reach_shell_stop(reach_shell *shell);
reach_result reach_shell_update(reach_shell *shell, double delta_seconds);

#ifdef __cplusplus
}
#endif

#endif
