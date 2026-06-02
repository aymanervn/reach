#ifndef REACHCTL_CONTEXT_MENU_H
#define REACHCTL_CONTEXT_MENU_H

#include "reach/support/util.h"

#include <stdint.h>

reach_result reachctl_install_context_menus(const uint16_t *reachctl_path,
                                            const uint16_t *reach_icon_path);

reach_result reachctl_remove_context_menus(void);

#endif
