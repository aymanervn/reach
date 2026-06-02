#ifndef REACHCTL_ELEVATION_HELPER_H
#define REACHCTL_ELEVATION_HELPER_H

#include "reach/support/util.h"

#include <stdint.h>

reach_result reachctl_install_elevation_helper(void);
reach_result reachctl_unregister_elevation_helper(void);
reach_result reachctl_start_elevation_helper(void);
int32_t reachctl_elevation_helper_is_installed(void);

#endif
