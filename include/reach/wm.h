#ifndef REACH_WM_H
#define REACH_WM_H

#include <stdint.h>

#include "reach/layout.h"
#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t reach_window_id;
typedef struct reach_wm reach_wm;

reach_result reach_wm_create(reach_wm **out_wm);
void reach_wm_destroy(reach_wm *wm);
reach_result reach_wm_install_hooks(reach_wm *wm);
reach_result reach_wm_uninstall_hooks(reach_wm *wm);
reach_result reach_wm_snap_window(reach_wm *wm, reach_window_id window, reach_split_mode mode);
reach_result reach_wm_update_z_order(reach_wm *wm);
reach_window_id reach_wm_foreground_window(const reach_wm *wm);

#ifdef __cplusplus
}
#endif

#endif
