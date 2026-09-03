#include "shortcut_win32.h"

#include <windows.h>
#include <shobjidl.h>

int32_t reach_windows_read_shortcut(const wchar_t *path,
                                    reach_windows_shortcut_info *out_info)
{
    if (path == nullptr || path[0] == 0 || out_info == nullptr)
    {
        return 0;
    }

    *out_info = {};
    HRESULT initialize = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int32_t uninitialize = SUCCEEDED(initialize) ? 1 : 0;
    if (FAILED(initialize) && initialize != RPC_E_CHANGED_MODE)
    {
        return 0;
    }

    IShellLinkW *link = nullptr;
    HRESULT hr =
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (SUCCEEDED(hr) && link != nullptr)
    {
        IPersistFile *persist = nullptr;
        hr = link->QueryInterface(IID_PPV_ARGS(&persist));
        if (SUCCEEDED(hr) && persist != nullptr)
        {
            hr = persist->Load(path, STGM_READ);
        }
        if (SUCCEEDED(hr))
        {
            (void)link->Resolve(nullptr, SLR_NO_UI | SLR_NOSEARCH | SLR_NOTRACK);
            (void)link->GetPath(out_info->target_path, 260, nullptr, SLGP_UNCPRIORITY);
            (void)link->GetArguments(out_info->arguments, 260);
            (void)link->GetIconLocation(out_info->icon_path, 260, &out_info->icon_index);
        }
        if (persist != nullptr)
        {
            persist->Release();
        }
        link->Release();
    }

    if (uninitialize)
    {
        CoUninitialize();
    }
    return SUCCEEDED(hr) && (out_info->target_path[0] != 0 || out_info->icon_path[0] != 0);
}
