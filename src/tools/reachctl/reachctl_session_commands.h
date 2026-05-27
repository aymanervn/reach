#ifndef REACHCTL_SESSION_COMMANDS_H
#define REACHCTL_SESSION_COMMANDS_H

#include "reach/support/util.h"

#include <stdint.h>

reach_result reachctl_install_reach_shell_and_watchdog(const uint16_t *reach_exe);
reach_result reachctl_start_reach_session(const uint16_t *reach_exe);
reach_result reachctl_reset_to_windows_shell(void);
reach_result reachctl_list_monitors(void);
int32_t reachctl_is_process_elevated(void);

#endif
