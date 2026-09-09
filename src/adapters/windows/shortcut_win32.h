#ifndef REACH_SHORTCUT_WIN32_H
#define REACH_SHORTCUT_WIN32_H

#include <stdint.h>

struct reach_windows_shortcut_info
{
    wchar_t target_path[260];
    wchar_t arguments[260];
    wchar_t icon_path[260];
    int32_t icon_index;
};

int32_t reach_windows_read_shortcut(const wchar_t *path, reach_windows_shortcut_info *out_info);

#endif
