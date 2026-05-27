#ifndef REACH_WINDOWS_ICON_HANDLE_INTERNAL_H
#define REACH_WINDOWS_ICON_HANDLE_INTERNAL_H

#include <windows.h>

#include <new>
#include <stdint.h>

#define REACH_WINDOWS_ICON_MAGIC 0x5249434Fu

enum reach_windows_icon_kind {
    REACH_WINDOWS_ICON_KIND_NONE = 0,
    REACH_WINDOWS_ICON_KIND_HICON = 1,
    REACH_WINDOWS_ICON_KIND_HBITMAP = 2
};

struct reach_windows_icon {
    uint32_t magic;
    reach_windows_icon_kind kind;
    HICON hicon;
    HBITMAP hbitmap;
};

static inline reach_windows_icon *reach_windows_icon_from_hicon(HICON hicon)
{
    if (hicon == nullptr) {
        return nullptr;
    }

    reach_windows_icon *icon = new (std::nothrow) reach_windows_icon();
    if (icon == nullptr) {
        DestroyIcon(hicon);
        return nullptr;
    }

    icon->magic = REACH_WINDOWS_ICON_MAGIC;
    icon->kind = REACH_WINDOWS_ICON_KIND_HICON;
    icon->hicon = hicon;
    icon->hbitmap = nullptr;
    return icon;
}

static inline reach_windows_icon *reach_windows_icon_from_hbitmap(HBITMAP hbitmap)
{
    if (hbitmap == nullptr) {
        return nullptr;
    }

    reach_windows_icon *icon = new (std::nothrow) reach_windows_icon();
    if (icon == nullptr) {
        DeleteObject(hbitmap);
        return nullptr;
    }

    icon->magic = REACH_WINDOWS_ICON_MAGIC;
    icon->kind = REACH_WINDOWS_ICON_KIND_HBITMAP;
    icon->hicon = nullptr;
    icon->hbitmap = hbitmap;
    return icon;
}

static inline void reach_windows_icon_destroy(reach_windows_icon *icon)
{
    if (icon == nullptr) {
        return;
    }

    icon->magic = 0;

    if (icon->hicon != nullptr) {
        DestroyIcon(icon->hicon);
    }
    if (icon->hbitmap != nullptr) {
        DeleteObject(icon->hbitmap);
    }

    delete icon;
}

#endif
