#ifndef REACH_ELEVATION_HELPER_TASK_WIN32_H
#define REACH_ELEVATION_HELPER_TASK_WIN32_H

#include "reach/support/util.h"

reach_result reach_elevation_helper_current_user_id(wchar_t *user_id, size_t user_id_count);
reach_result reach_elevation_helper_task_register(const wchar_t *helper_path,
                                                  const wchar_t *user_id);
reach_result reach_elevation_helper_task_run(void);
int32_t reach_elevation_helper_task_valid(const wchar_t *helper_path);
reach_result reach_elevation_helper_task_unregister(void);

#endif
