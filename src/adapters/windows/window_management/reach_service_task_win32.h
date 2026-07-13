#ifndef REACH_SERVICE_TASK_WIN32_H
#define REACH_SERVICE_TASK_WIN32_H

#include "reach/support/util.h"

reach_result reach_service_current_user_id(wchar_t *user_id, size_t user_id_count);
reach_result reach_service_task_register(const wchar_t *helper_path, const wchar_t *user_id);
reach_result reach_service_task_run(void);
int32_t reach_service_task_valid(const wchar_t *helper_path);
reach_result reach_service_task_unregister(void);

#endif
