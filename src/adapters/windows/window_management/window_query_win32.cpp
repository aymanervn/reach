#include "window_query_win32.h"

#include <appmodel.h>
#include <propvarutil.h>

int32_t reach_window_property_string(IPropertyStore *store, const PROPERTYKEY &key,
                                     uint16_t *out_value, size_t out_count)
{
    if (store == nullptr || out_value == nullptr || out_count == 0)
    {
        return 0;
    }

    out_value[0] = 0;

    PROPVARIANT value = {};
    PropVariantInit(&value);

    HRESULT hr = store->GetValue(key, &value);

    int32_t ok = 0;
    if (SUCCEEDED(hr) && value.vt == VT_LPWSTR && value.pwszVal != nullptr && value.pwszVal[0] != 0)
    {
        ok = reach_copy_utf16(out_value, out_count,
                              reinterpret_cast<const uint16_t *>(value.pwszVal)) == REACH_OK;
    }

    PropVariantClear(&value);
    return ok;
}

int32_t reach_window_property_string(HWND hwnd, const PROPERTYKEY &key, uint16_t *out_value,
                                     size_t out_count)
{
    if (hwnd == nullptr || out_value == nullptr || out_count == 0)
    {
        return 0;
    }

    out_value[0] = 0;

    IPropertyStore *store = nullptr;
    HRESULT hr = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store));
    if (FAILED(hr) || store == nullptr)
    {
        return 0;
    }

    int32_t ok = reach_window_property_string(store, key, out_value, out_count);
    store->Release();
    return ok;
}

int32_t reach_window_app_user_model_id(HWND hwnd, uint16_t *out_id, size_t out_count)
{
    return reach_window_property_string(hwnd, PKEY_AppUserModel_ID, out_id, out_count);
}

int32_t reach_window_process_app_user_model_id_for_process(DWORD process_id, uint16_t *out_id,
                                                           size_t out_count)
{
    if (process_id == 0 || out_id == nullptr || out_count == 0)
    {
        return 0;
    }

    out_id[0] = 0;

    if (process_id == GetCurrentProcessId())
    {
        return 0;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        return 0;
    }

    UINT32 length = 0;
    LONG status = GetApplicationUserModelId(process, &length, nullptr);

    if (status != ERROR_INSUFFICIENT_BUFFER || length == 0)
    {
        CloseHandle(process);
        return 0;
    }

    wchar_t app_id[260] = {};
    UINT32 app_id_count = 260;

    status = GetApplicationUserModelId(process, &app_id_count, app_id);

    CloseHandle(process);

    if (status != ERROR_SUCCESS || app_id[0] == 0)
    {
        return 0;
    }

    return reach_copy_utf16(out_id, out_count, reinterpret_cast<const uint16_t *>(app_id)) ==
           REACH_OK;
}

int32_t reach_window_process_app_user_model_id(HWND hwnd, uint16_t *out_id, size_t out_count)
{
    if (hwnd == nullptr || out_id == nullptr || out_count == 0)
    {
        return 0;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    return reach_window_process_app_user_model_id_for_process(process_id, out_id, out_count);
}

int32_t reach_window_query_process_path(HWND hwnd, uint16_t *out_path, size_t out_path_count)
{
    if (out_path == nullptr || out_path_count == 0)
    {
        return 0;
    }
    out_path[0] = 0;
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    return reach_window_query_process_path_for_process(process_id, out_path, out_path_count);
}

int32_t reach_window_query_process_path_for_process(DWORD process_id, uint16_t *out_path,
                                                    size_t out_path_count)
{
    if (out_path == nullptr || out_path_count == 0)
    {
        return 0;
    }
    out_path[0] = 0;
    if (process_id == 0 || process_id == GetCurrentProcessId())
    {
        return 0;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        return 0;
    }
    wchar_t path[260] = {};
    DWORD path_count = 260;
    BOOL ok = QueryFullProcessImageNameW(process, 0, path, &path_count);
    CloseHandle(process);
    if (!ok || path[0] == 0)
    {
        return 0;
    }
    return reach_copy_utf16(out_path, out_path_count, reinterpret_cast<const uint16_t *>(path)) ==
           REACH_OK;
}
