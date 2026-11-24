#ifndef REACHCTL_SESSION_COMMANDS_H
#define REACHCTL_SESSION_COMMANDS_H

#include "reach/support/util.h"

#include <stdint.h>

reach_result reachctl_install_reach_shell(const uint16_t *reach_exe);
int32_t reachctl_reach_is_running(void);
reach_result reachctl_start_reach_session(const uint16_t *reach_exe);
reach_result reachctl_reset_to_windows_shell(void);
reach_result reachctl_list_monitors(void);
int32_t reachctl_is_reach_installed(void);

#endif
