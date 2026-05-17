#ifndef REACH_PLATFORM_SHELL_REGISTRATION_H
#define REACH_PLATFORM_SHELL_REGISTRATION_H

#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_shell_registration_status {
    uint16_t current_shell[260];
    uint16_t previous_shell[260];
    int32_t reach_is_shell;
} reach_shell_registration_status;

reach_result reach_windows_shell_install_current_user(const uint16_t *exe_path);
reach_result reach_windows_shell_restore_current_user(void);
reach_result reach_windows_shell_query_current_user(const uint16_t *exe_path, reach_shell_registration_status *out_status);
reach_result reach_windows_shell_launch_explorer(void);

#ifdef __cplusplus
}
#endif

#endif
