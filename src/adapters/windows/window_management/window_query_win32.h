#ifndef REACH_WINDOW_QUERY_WIN32_H
#define REACH_WINDOW_QUERY_WIN32_H

#include "../windows_adapters_internal.h"

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <shellapi.h>

int32_t reach_window_property_string(IPropertyStore *store, const PROPERTYKEY &key,
                                     uint16_t *out_value, size_t out_count);
int32_t reach_window_property_string(HWND hwnd, const PROPERTYKEY &key, uint16_t *out_value,
                                     size_t out_count);
int32_t reach_window_app_user_model_id(HWND hwnd, uint16_t *out_id, size_t out_count);
int32_t reach_window_process_app_user_model_id_for_process(DWORD process_id, uint16_t *out_id,
                                                           size_t out_count);
int32_t reach_window_process_app_user_model_id(HWND hwnd, uint16_t *out_id, size_t out_count);
int32_t reach_window_query_process_path(HWND hwnd, uint16_t *out_path, size_t out_path_count);
int32_t reach_window_query_process_path_for_process(DWORD process_id, uint16_t *out_path,
                                                    size_t out_path_count);

#endif
