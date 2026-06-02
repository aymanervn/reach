// Build: cl /std:c++17 /EHsc /DUNICODE /D_UNICODE reach_desktop_probe.cpp user32.lib gdi32.lib
// ole32.lib shell32.lib shlwapi.lib psapi.lib shcore.lib dwmapi.lib wtsapi32.lib

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include <shellscalingapi.h>
#include <commctrl.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "comctl32.lib")

struct Config
{
    bool watch = false;
    bool dumpTree = true;
    bool dumpWallpaper = true;
    bool dumpMonitors = true;
    bool dumpProcesses = true;
    bool createHosts = false;
    bool createExplorerShim = false;
    bool pokeProgman052c = false;
    bool shellHook = true;
    DWORD intervalMs = 3000;
    std::wstring outPath = L"reach_desktop_probe.jsonl";
};

static Config g_cfg;
static FILE *g_log = nullptr;
static UINT g_shellMsg = 0;
static std::vector<HWND> g_hosts;
static std::vector<HWND> g_shimWindows;

static std::wstring NowIso()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04u-%02u-%02uT%02u:%02u:%02u.%03u", st.wYear, st.wMonth, st.wDay, st.wHour,
               st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

static std::wstring JsonEscape(const std::wstring &s)
{
    std::wstring o;
    o.reserve(s.size() + 16);
    for (wchar_t c : s)
    {
        switch (c)
        {
        case L'\\':
            o += L"\\\\";
            break;
        case L'\"':
            o += L"\\\"";
            break;
        case L'\n':
            o += L"\\n";
            break;
        case L'\r':
            o += L"\\r";
            break;
        case L'\t':
            o += L"\\t";
            break;
        default:
            if (c < 0x20)
            {
                wchar_t b[8];
                swprintf_s(b, L"\\u%04x", c);
                o += b;
            }
            else
                o += c;
        }
    }
    return o;
}

static std::wstring HwndHex(HWND h)
{
    wchar_t buf[32];
    swprintf_s(buf, L"0x%p", h);
    return buf;
}
static std::wstring U64Hex(ULONG_PTR v)
{
    wchar_t buf[32];
    swprintf_s(buf, L"0x%016llX", (unsigned long long)v);
    return buf;
}
static std::wstring RectStr(const RECT &r)
{
    wchar_t b[128];
    swprintf_s(b, L"[%ld,%ld,%ld,%ld %ldx%ld]", r.left, r.top, r.right, r.bottom, r.right - r.left,
               r.bottom - r.top);
    return b;
}
static void LogRaw(const std::wstring &line)
{
    if (!g_log)
        return;
    fwprintf(g_log, L"%s\n", line.c_str());
    fflush(g_log);
    wprintf(L"%s\n", line.c_str());
}
static void LogEvent(const wchar_t *type, const std::wstring &body)
{
    std::wstring line = L"{\"time\":\"" + JsonEscape(NowIso()) + L"\",\"type\":\"" + type + L"\"";
    if (!body.empty())
        line += L"," + body;
    line += L"}";
    LogRaw(line);
}

static std::wstring GetClass(HWND h)
{
    wchar_t c[256]{};
    GetClassNameW(h, c, 256);
    return c;
}
static std::wstring GetText(HWND h)
{
    int n = GetWindowTextLengthW(h);
    if (n <= 0)
        return L"";
    n = std::min<int>(n, 1024);
    std::wstring s((size_t)n + 1, L'\0');
    int copied = GetWindowTextW(h, &s[0], n + 1);
    if (copied < 0)
        copied = 0;
    s.resize((size_t)copied);
    return s;
}
static std::wstring ProcessPath(DWORD pid)
{
    std::wstring res;
    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hp)
        return L"";
    wchar_t buf[MAX_PATH * 4]{};
    DWORD sz = _countof(buf);
    if (QueryFullProcessImageNameW(hp, 0, buf, &sz))
        res = buf;
    CloseHandle(hp);
    return res;
}
static std::wstring BaseName(const std::wstring &path)
{
    const wchar_t *p = PathFindFileNameW(path.c_str());
    return p ? p : path;
}
static std::wstring StyleNames(DWORD_PTR s, bool ex)
{
    struct Bit
    {
        DWORD_PTR v;
        const wchar_t *n;
    };
    static const Bit style[] = {{WS_CHILD, L"WS_CHILD"},
                                {WS_POPUP, L"WS_POPUP"},
                                {WS_VISIBLE, L"WS_VISIBLE"},
                                {WS_DISABLED, L"WS_DISABLED"},
                                {WS_CLIPCHILDREN, L"WS_CLIPCHILDREN"},
                                {WS_CLIPSIBLINGS, L"WS_CLIPSIBLINGS"},
                                {WS_CAPTION, L"WS_CAPTION"},
                                {WS_THICKFRAME, L"WS_THICKFRAME"},
                                {WS_MINIMIZE, L"WS_MINIMIZE"},
                                {WS_MAXIMIZE, L"WS_MAXIMIZE"}};
    static const Bit exstyle[] = {{WS_EX_TOPMOST, L"WS_EX_TOPMOST"},
                                  {WS_EX_TOOLWINDOW, L"WS_EX_TOOLWINDOW"},
                                  {WS_EX_APPWINDOW, L"WS_EX_APPWINDOW"},
                                  {WS_EX_NOACTIVATE, L"WS_EX_NOACTIVATE"},
                                  {WS_EX_LAYERED, L"WS_EX_LAYERED"},
                                  {WS_EX_TRANSPARENT, L"WS_EX_TRANSPARENT"},
                                  {WS_EX_NOREDIRECTIONBITMAP, L"WS_EX_NOREDIRECTIONBITMAP"},
                                  {WS_EX_COMPOSITED, L"WS_EX_COMPOSITED"},
                                  {WS_EX_ACCEPTFILES, L"WS_EX_ACCEPTFILES"}};
    const Bit *arr = ex ? exstyle : style;
    const size_t count =
        ex ? (sizeof(exstyle) / sizeof(exstyle[0])) : (sizeof(style) / sizeof(style[0]));
    std::wstring out;
    for (size_t i = 0; i < count; ++i)
    {
        const Bit &b = arr[i];
        if ((s & b.v) == b.v)
        {
            if (!out.empty())
                out += L"|";
            out += b.n;
        }
    }
    return out;
}
static BOOL CALLBACK ChildCounter(HWND, LPARAM lp)
{
    (*(int *)lp)++;
    return TRUE;
}
static int CountChildren(HWND h)
{
    int c = 0;
    EnumChildWindows(h, ChildCounter, (LPARAM)&c);
    return c;
}

static void DumpWindow(HWND h, int depth, int zIndex, const wchar_t *reason)
{
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(h, &pid);
    RECT wr{};
    GetWindowRect(h, &wr);
    RECT cr{};
    GetClientRect(h, &cr);
    DWORD_PTR style = (DWORD_PTR)GetWindowLongPtrW(h, GWL_STYLE);
    DWORD_PTR ex = (DWORD_PTR)GetWindowLongPtrW(h, GWL_EXSTYLE);
    BOOL cloaked = FALSE;
    DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    std::wstring path = ProcessPath(pid);
    std::wstringstream ss;
    ss << L"\"reason\":\"" << reason << L"\"," << L"\"hwnd\":\"" << HwndHex(h) << L"\","
       << L"\"depth\":" << depth << L",\"zIndex\":" << zIndex << L"," << L"\"class\":\""
       << JsonEscape(GetClass(h)) << L"\"," << L"\"title\":\"" << JsonEscape(GetText(h)) << L"\","
       << L"\"pid\":" << pid << L",\"tid\":" << tid << L"," << L"\"process\":\""
       << JsonEscape(BaseName(path)) << L"\",\"path\":\"" << JsonEscape(path) << L"\","
       << L"\"parent\":\"" << HwndHex(GetParent(h)) << L"\",\"owner\":\""
       << HwndHex(GetWindow(h, GW_OWNER)) << L"\"," << L"\"root\":\""
       << HwndHex(GetAncestor(h, GA_ROOT)) << L"\",\"rootOwner\":\""
       << HwndHex(GetAncestor(h, GA_ROOTOWNER)) << L"\"," << L"\"visible\":"
       << (IsWindowVisible(h) ? L"true" : L"false") << L",\"enabled\":"
       << (IsWindowEnabled(h) ? L"true" : L"false") << L",\"iconic\":"
       << (IsIconic(h) ? L"true" : L"false") << L",\"cloaked\":" << (cloaked ? L"true" : L"false")
       << L"," << L"\"rect\":\"" << RectStr(wr) << L"\",\"client\":\"" << RectStr(cr) << L"\","
       << L"\"styleHex\":\"" << U64Hex(style) << L"\",\"style\":\"" << StyleNames(style, false)
       << L"\"," << L"\"exStyleHex\":\"" << U64Hex(ex) << L"\",\"exStyle\":\""
       << StyleNames(ex, true) << L"\"," << L"\"childCount\":" << CountChildren(h);
    LogEvent(L"window", ss.str());
}

static bool IsInteresting(HWND h)
{
    std::wstring cls = GetClass(h);
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    std::wstring exe = BaseName(ProcessPath(pid));
    std::transform(exe.begin(), exe.end(), exe.begin(), ::towlower);
    return cls == L"Progman" || cls == L"WorkerW" || cls == L"SHELLDLL_DefView" ||
           cls == L"SysListView32" || exe.find(L"wallpaper") != std::wstring::npos ||
           exe.find(L"webwallpaper") != std::wstring::npos || exe == L"explorer.exe" ||
           h == GetShellWindow();
}

static void DumpChildrenRecursive(HWND parent, int depth)
{
    struct Ctx
    {
        int depth;
    } ctx{depth};
    EnumChildWindows(
        parent,
        [](HWND h, LPARAM lp) -> BOOL
        {
            Ctx *c = (Ctx *)lp;
            if (IsInteresting(h))
                DumpWindow(h, c->depth, -1, L"interesting-child");
            DumpChildrenRecursive(h, c->depth + 1);
            return TRUE;
        },
        (LPARAM)&ctx);
}

static void DumpTopology(const wchar_t *reason)
{
    HWND shell = GetShellWindow();
    HWND desktop = GetDesktopWindow();
    HWND progman = FindWindowW(L"Progman", nullptr);
    HWND defView = nullptr;
    if (progman)
        defView = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
    HWND worker = nullptr;
    while ((worker = FindWindowExW(nullptr, worker, L"WorkerW", nullptr)) != nullptr)
    {
        HWND childDef = FindWindowExW(worker, nullptr, L"SHELLDLL_DefView", nullptr);
        if (childDef)
            defView = childDef;
    }
    std::wstringstream ss;
    ss << L"\"reason\":\"" << reason << L"\",\"GetShellWindow\":\"" << HwndHex(shell)
       << L"\",\"GetDesktopWindow\":\"" << HwndHex(desktop) << L"\",\"FindWindowProgman\":\""
       << HwndHex(progman) << L"\",\"DefView\":\"" << HwndHex(defView) << L"\"";
    LogEvent(L"topology_summary", ss.str());

    int z = 0;
    for (HWND h = GetTopWindow(nullptr); h; h = GetWindow(h, GW_HWNDNEXT), ++z)
    {
        if (IsInteresting(h) || z < 12)
            DumpWindow(h, 0, z, reason);
        if (IsInteresting(h))
            DumpChildrenRecursive(h, 1);
    }
}

static void DumpProcesses()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;
    PROCESSENTRY32W pe{sizeof(pe)};
    if (Process32FirstW(snap, &pe))
        do
        {
            std::wstring exe = pe.szExeFile;
            std::wstring lower = exe;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            if (lower.find(L"wallpaper") != std::wstring::npos || lower == L"explorer.exe" ||
                lower == L"reach.exe")
            {
                std::wstringstream ss;
                ss << L"\"pid\":" << pe.th32ProcessID << L",\"parentPid\":"
                   << pe.th32ParentProcessID << L",\"exe\":\"" << JsonEscape(exe)
                   << L"\",\"path\":\"" << JsonEscape(ProcessPath(pe.th32ProcessID)) << L"\"";
                LogEvent(L"process", ss.str());
            }
        } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR hm, HDC, LPRECT, LPARAM)
{
    MONITORINFOEXW mi{sizeof(mi)};
    GetMonitorInfoW(hm, &mi);
    UINT dpiX = 0, dpiY = 0;
    auto pGetDpiForMonitor = (decltype(&GetDpiForMonitor))GetProcAddress(
        GetModuleHandleW(L"shcore.dll"), "GetDpiForMonitor");
    if (pGetDpiForMonitor)
        pGetDpiForMonitor(hm, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    std::wstringstream ss;
    ss << L"\"hmonitor\":\"" << HwndHex((HWND)hm) << L"\",\"device\":\"" << JsonEscape(mi.szDevice)
       << L"\"," << L"\"monitorRect\":\"" << RectStr(mi.rcMonitor) << L"\",\"workRect\":\""
       << RectStr(mi.rcWork) << L"\"," << L"\"primary\":"
       << ((mi.dwFlags & MONITORINFOF_PRIMARY) ? L"true" : L"false") << L",\"dpiX\":" << dpiX
       << L",\"dpiY\":" << dpiY;
    LogEvent(L"monitor", ss.str());
    return TRUE;
}
static void DumpMonitors()
{
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, 0);
}

static void DumpDesktopWallpaperApi()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool co = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    IDesktopWallpaper *dw = nullptr;
    hr = CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dw));
    if (FAILED(hr))
    {
        wchar_t b[64];
        swprintf_s(b, L"\"hr\":\"0x%08X\"", (unsigned)hr);
        LogEvent(L"idesktopwallpaper_error", b);
        return;
    }
    UINT count = 0;
    dw->GetMonitorDevicePathCount(&count);
    std::wstringstream sum;
    sum << L"\"monitorCount\":" << count;
    LogEvent(L"idesktopwallpaper_summary", sum.str());
    for (UINT i = 0; i < count; i++)
    {
        LPWSTR id = nullptr, wp = nullptr;
        RECT r{};
        HRESULT hid = dw->GetMonitorDevicePathAt(i, &id);
        if (SUCCEEDED(hid) && id)
        {
            dw->GetMonitorRECT(id, &r);
            dw->GetWallpaper(id, &wp);
            std::wstringstream ss;
            ss << L"\"index\":" << i << L",\"monitorId\":\"" << JsonEscape(id) << L"\",\"rect\":\""
               << RectStr(r) << L"\",\"wallpaper\":\"" << JsonEscape(wp ? wp : L"") << L"\"";
            LogEvent(L"idesktopwallpaper_monitor", ss.str());
        }
        if (id)
            CoTaskMemFree(id);
        if (wp)
            CoTaskMemFree(wp);
    }
    dw->Release();
    if (co && SUCCEEDED(hr))
        CoUninitialize();
}

static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == g_shellMsg)
    {
        std::wstringstream ss;
        ss << L"\"hwnd\":\"" << HwndHex(hwnd) << L"\",\"code\":" << (long long)wp << L",\"lParam\":"
           << (long long)lp;
        LogEvent(L"shell_hook", ss.str());
        return 0;
    }
    switch (msg)
    {
    case WM_DISPLAYCHANGE:
    {
        std::wstringstream ss;
        ss << L"\"bitsPerPixel\":" << (int)wp << L",\"width\":" << LOWORD(lp) << L",\"height\":"
           << HIWORD(lp);
        LogEvent(L"WM_DISPLAYCHANGE", ss.str());
        DumpMonitors();
        DumpTopology(L"after-WM_DISPLAYCHANGE");
        break;
    }
    case WM_SETTINGCHANGE:
    {
        std::wstringstream ss;
        ss << L"\"wParam\":" << (unsigned)wp << L",\"lParamString\":\""
           << JsonEscape(lp ? (LPCWSTR)lp : L"") << L"\"";
        LogEvent(L"WM_SETTINGCHANGE", ss.str());
        break;
    }
    case WM_DPICHANGED:
    {
        RECT *pr = (RECT *)lp;
        std::wstringstream ss;
        ss << L"\"dpiX\":" << LOWORD(wp) << L",\"dpiY\":" << HIWORD(wp) << L",\"suggested\":\""
           << (pr ? RectStr(*pr) : L"") << L"\"";
        LogEvent(L"WM_DPICHANGED", ss.str());
        if (pr)
            SetWindowPos(hwnd, nullptr, pr->left, pr->top, pr->right - pr->left,
                         pr->bottom - pr->top, SWP_NOZORDER | SWP_NOACTIVATE);
        break;
    }
    case WM_POWERBROADCAST:
    {
        std::wstringstream ss;
        ss << L"\"event\":" << (unsigned)wp;
        LogEvent(L"WM_POWERBROADCAST", ss.str());
        break;
    }
    case WM_DEVICECHANGE:
    {
        std::wstringstream ss;
        ss << L"\"event\":" << (unsigned)wp;
        LogEvent(L"WM_DEVICECHANGE", ss.str());
        break;
    }
    case WM_WTSSESSION_CHANGE:
    {
        std::wstringstream ss;
        ss << L"\"event\":" << (unsigned)wp << L",\"session\":" << (unsigned)lp;
        LogEvent(L"WM_WTSSESSION_CHANGE", ss.str());
        break;
    }
    case WM_DWMCOMPOSITIONCHANGED:
        LogEvent(L"WM_DWMCOMPOSITIONCHANGED", L"");
        break;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static ATOM RegisterProbeClass()
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = HostWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ReachDesktopProbeHost";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_DESKTOP + 1);
    return RegisterClassW(&wc);
}

static BOOL CALLBACK CreateHostPerMonitorProc(HMONITOR, HDC, LPRECT rc, LPARAM)
{
    HWND h = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"ReachDesktopProbeHost",
                             L"Reach Wallpaper Host Probe",
                             WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rc->left,
                             rc->top, rc->right - rc->left, rc->bottom - rc->top, nullptr, nullptr,
                             GetModuleHandleW(nullptr), nullptr);
    if (h)
    {
        SetWindowPos(h, HWND_BOTTOM, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        g_hosts.push_back(h);
        std::wstringstream ss;
        ss << L"\"hwnd\":\"" << HwndHex(h) << L"\",\"rect\":\"" << RectStr(*rc) << L"\"";
        LogEvent(L"created_host", ss.str());
    }
    return TRUE;
}

static ATOM RegisterNamedProbeClass(const wchar_t *className)
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = HostWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_DESKTOP + 1);
    ATOM a = RegisterClassW(&wc);
    std::wstringstream ss;
    ss << L"\"class\":\"" << JsonEscape(className) << L"\",\"atom\":" << (unsigned)a
       << L",\"lastError\":" << GetLastError();
    LogEvent(L"register_shim_class", ss.str());
    return a;
}

static RECT VirtualScreenRect()
{
    RECT r{};
    r.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    r.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    r.right = r.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    r.bottom = r.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return r;
}

static void CreateExplorerShim()
{
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);

    RegisterNamedProbeClass(L"Progman");
    RegisterNamedProbeClass(L"WorkerW");
    RegisterNamedProbeClass(L"SHELLDLL_DefView");

    RECT vr = VirtualScreenRect();
    HINSTANCE inst = GetModuleHandleW(nullptr);

    HWND progman = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP, L"Progman", L"Program Manager",
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, vr.left, vr.top,
        vr.right - vr.left, vr.bottom - vr.top, nullptr, nullptr, inst, nullptr);

    if (!progman)
    {
        std::wstringstream ss;
        ss << L"\"stage\":\"CreateWindow Progman\",\"lastError\":" << GetLastError();
        LogEvent(L"create_explorer_shim_error", ss.str());
        return;
    }
    g_shimWindows.push_back(progman);
    SetWindowPos(progman, HWND_BOTTOM, vr.left, vr.top, vr.right - vr.left, vr.bottom - vr.top,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    HWND defview =
        CreateWindowExW(WS_EX_LAYERED, L"SHELLDLL_DefView", L"",
                        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0,
                        vr.right - vr.left, vr.bottom - vr.top, progman, nullptr, inst, nullptr);
    if (defview)
        g_shimWindows.push_back(defview);

    HWND listview = CreateWindowExW(0, WC_LISTVIEWW, L"FolderView",
                                    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0,
                                    vr.right - vr.left, vr.bottom - vr.top,
                                    defview ? defview : progman, nullptr, inst, nullptr);
    if (listview)
        g_shimWindows.push_back(listview);

    HWND worker =
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, L"WorkerW", L"",
                        WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0,
                        0, vr.right - vr.left, vr.bottom - vr.top, progman, nullptr, inst, nullptr);
    if (worker)
        g_shimWindows.push_back(worker);

    if (defview)
        SetWindowPos(defview, HWND_TOP, 0, 0, vr.right - vr.left, vr.bottom - vr.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (worker)
        SetWindowPos(worker, HWND_BOTTOM, 0, 0, vr.right - vr.left, vr.bottom - vr.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);

    std::wstringstream ss;
    ss << L"\"progman\":\"" << HwndHex(progman) << L"\",\"defView\":\"" << HwndHex(defview)
       << L"\",\"listView\":\"" << HwndHex(listview) << L"\",\"workerW\":\"" << HwndHex(worker)
       << L"\",\"rect\":\"" << RectStr(vr) << L"\"";
    LogEvent(L"created_explorer_shim", ss.str());
}

static void Usage()
{
    wprintf(L"Reach desktop probe\n");
    wprintf(L"Options:\n");
    wprintf(L"  --out <file.jsonl>         output log path\n");
    wprintf(L"  --watch                   keep running and sample topology periodically\n");
    wprintf(L"  --interval-ms <n>         watch interval, default 3000\n");
    wprintf(L"  --host                    create non-activating bottom-most host windows per "
            L"monitor\n");
    wprintf(L"  --explorer-shim           create fake "
            L"Progman/SHELLDLL_DefView/SysListView32/WorkerW topology\n");
    wprintf(L"  --poke-progman-052c       send private Progman 0x052C using SendMessageTimeout; "
            L"diagnostic only\n");
    wprintf(L"  --no-shell-hook           do not call RegisterShellHookWindow\n");
}

static void ParseArgs(int argc, wchar_t **argv)
{
    for (int i = 1; i < argc; i++)
    {
        std::wstring a = argv[i];
        if (a == L"--help" || a == L"-h")
        {
            Usage();
            ExitProcess(0);
        }
        else if (a == L"--out" && i + 1 < argc)
            g_cfg.outPath = argv[++i];
        else if (a == L"--watch")
            g_cfg.watch = true;
        else if (a == L"--interval-ms" && i + 1 < argc)
            g_cfg.intervalMs = wcstoul(argv[++i], nullptr, 10);
        else if (a == L"--host")
            g_cfg.createHosts = true;
        else if (a == L"--explorer-shim")
            g_cfg.createExplorerShim = true;
        else if (a == L"--poke-progman-052c")
            g_cfg.pokeProgman052c = true;
        else if (a == L"--no-shell-hook")
            g_cfg.shellHook = false;
    }
}

int wmain(int argc, wchar_t **argv)
{
    ParseArgs(argc, argv);
    _wfopen_s(&g_log, g_cfg.outPath.c_str(), L"w, ccs=UTF-8");
    if (!g_log)
    {
        fwprintf(stderr, L"Failed to open log: %s\n", g_cfg.outPath.c_str());
        return 2;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RegisterProbeClass();
    g_shellMsg = RegisterWindowMessageW(L"SHELLHOOK");

    HWND msgWnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"ReachDesktopProbeHost",
                                  L"Reach Desktop Probe Message Window", WS_POPUP, 0, 0, 1, 1,
                                  nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (msgWnd)
    {
        WTSRegisterSessionNotification(msgWnd, NOTIFY_FOR_THIS_SESSION);
        if (g_cfg.shellHook)
        {
            BOOL ok = RegisterShellHookWindow(msgWnd);
            std::wstringstream ss;
            ss << L"\"hwnd\":\"" << HwndHex(msgWnd) << L"\",\"ok\":" << (ok ? L"true" : L"false")
               << L",\"lastError\":" << GetLastError();
            LogEvent(L"RegisterShellHookWindow", ss.str());
        }
    }

    LogEvent(L"start", L"\"program\":\"reach_desktop_probe\"");
    if (g_cfg.createExplorerShim)
        CreateExplorerShim();
    if (g_cfg.dumpMonitors)
        DumpMonitors();
    if (g_cfg.dumpWallpaper)
        DumpDesktopWallpaperApi();
    if (g_cfg.dumpProcesses)
        DumpProcesses();
    if (g_cfg.dumpTree)
        DumpTopology(L"initial");

    if (g_cfg.pokeProgman052c)
    {
        HWND progman = FindWindowW(L"Progman", nullptr);
        DWORD_PTR result = 0;
        LRESULT ok = SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL,
                                         2000, &result);
        std::wstringstream ss;
        ss << L"\"progman\":\"" << HwndHex(progman) << L"\",\"sendOk\":"
           << (ok ? L"true" : L"false") << L",\"result\":" << (unsigned long long)result
           << L",\"lastError\":" << GetLastError();
        LogEvent(L"poke_progman_052c", ss.str());
        DumpTopology(L"after-poke-progman-052c");
    }

    if (g_cfg.createHosts)
        EnumDisplayMonitors(nullptr, nullptr, CreateHostPerMonitorProc, 0);

    MSG msg{};
    auto last = GetTickCount64();
    while (g_cfg.watch)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        ULONGLONG now = GetTickCount64();
        if (now - last >= g_cfg.intervalMs)
        {
            last = now;
            DumpProcesses();
            DumpTopology(L"watch");
        }
        Sleep(30);
    }

    if (!g_cfg.watch)
    {
        // Pump briefly so messages generated during startup are captured.
        DWORD until = GetTickCount() + 500;
        while (GetTickCount() < until)
        {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(10);
        }
    }
    if (msgWnd)
        WTSUnRegisterSessionNotification(msgWnd);
    LogEvent(L"stop", L"");
    if (g_log)
        fclose(g_log);
    return 0;
}
