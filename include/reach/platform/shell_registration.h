#ifndef REACH_PLATFORM_SHELL_REGISTRATION_H
#define REACH_PLATFORM_SHELL_REGISTRATION_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_shell_registration_status {
    uint16_t current_shell[260];
    uint16_t previous_shell[260];
    int32_t reach_is_shell;
    uint32_t startup_attempt_count;
} reach_shell_registration_status;

reach_result reach_windows_shell_install_current_user(const uint16_t *exe_path);
reach_result reach_windows_shell_restore_current_user(void);
reach_result reach_windows_shell_query_current_user(const uint16_t *exe_path, reach_shell_registration_status *out_status);
reach_result reach_windows_shell_launch_explorer(void);
reach_result reach_windows_shell_mark_startup_attempt(const uint16_t *exe_path, uint32_t *out_attempt_count, int32_t *out_restore_required);
reach_result reach_windows_shell_clear_startup_attempts(void);

#ifdef __cplusplus
}
#endif

#endif
