#ifndef REACHCTL_COMMON_H
#define REACHCTL_COMMON_H

#include "reach/support/util.h"
#include "reach/ports/config_store.h"

#include <windows.h>
#include <stdint.h>

void reachctl_print(const wchar_t *message);

reach_result reachctl_target_exe(uint16_t *path, DWORD path_count);
reach_result reachctl_current_exe(uint16_t *path, DWORD path_count);
reach_result reachctl_open_config_store(reach_config_store_port *out_store);
reach_result reachctl_absolute_path(
    const uint16_t *path,
    uint16_t *out_path,
    DWORD out_path_count);
reach_result reachctl_run_process_wait(
    const wchar_t *path,
    const wchar_t *arguments,
    const wchar_t *working_directory);

reach_result reachctl_notify_config_changed(void);

int32_t reachctl_path_equals_ci(const uint16_t *a, const uint16_t *b);

#endif
