#ifndef REACH_WINDOWS_ICON_HANDLE_INTERNAL_H
#define REACH_WINDOWS_ICON_HANDLE_INTERNAL_H

#include <windows.h>

#include <atomic>
#include <stdint.h>

#define REACH_WINDOWS_ICON_MAGIC 0x5249434Fu

enum reach_windows_icon_kind
{
    REACH_WINDOWS_ICON_KIND_NONE = 0,
    REACH_WINDOWS_ICON_KIND_HICON = 1,
    REACH_WINDOWS_ICON_KIND_HBITMAP = 2
};

struct reach_windows_icon
{
    std::atomic<uint32_t> references;
    uint32_t magic;
    reach_windows_icon_kind kind;
    HICON hicon;
    HBITMAP hbitmap;
};

uint64_t reach_windows_icon_id_from_hicon(HICON hicon);
uint64_t reach_windows_icon_id_from_hbitmap(HBITMAP hbitmap);
void reach_windows_icon_id_retain(uint64_t icon_id);
void reach_windows_icon_id_release(uint64_t icon_id);

#endif
