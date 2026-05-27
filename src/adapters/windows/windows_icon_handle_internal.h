#ifndef REACH_WINDOWS_ICON_HANDLE_INTERNAL_H
#define REACH_WINDOWS_ICON_HANDLE_INTERNAL_H

#include <windows.h>

enum reach_windows_icon_kind {
    REACH_WINDOWS_ICON_KIND_NONE = 0,
    REACH_WINDOWS_ICON_KIND_HICON = 1,
    REACH_WINDOWS_ICON_KIND_HBITMAP = 2
};

struct reach_windows_icon {
    reach_windows_icon_kind kind;
    HICON hicon;
    HBITMAP hbitmap;
};

#endif
