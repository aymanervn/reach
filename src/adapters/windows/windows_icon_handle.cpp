#include "windows_icon_handle_internal.h"

#include <new>

static reach_windows_icon *reach_windows_icon_from_hicon(HICON hicon)
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

static reach_windows_icon *reach_windows_icon_from_hbitmap(HBITMAP hbitmap)
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

static void reach_windows_icon_destroy(reach_windows_icon *icon)
{
    if (icon == nullptr) {
        return;
    }

    icon->magic = 0;

    if (icon->hicon != nullptr) {
        DestroyIcon(icon->hicon);
        icon->hicon = nullptr;
    }

    if (icon->hbitmap != nullptr) {
        DeleteObject(icon->hbitmap);
        icon->hbitmap = nullptr;
    }

    delete icon;
}

uint64_t reach_windows_icon_id_from_hicon(HICON hicon)
{
    reach_windows_icon *icon = reach_windows_icon_from_hicon(hicon);
    return icon != nullptr ? reinterpret_cast<uint64_t>(icon) : 0;
}

uint64_t reach_windows_icon_id_from_hbitmap(HBITMAP hbitmap)
{
    reach_windows_icon *icon = reach_windows_icon_from_hbitmap(hbitmap);
    return icon != nullptr ? reinterpret_cast<uint64_t>(icon) : 0;
}

void reach_windows_icon_id_release(uint64_t icon_id)
{
    if (icon_id == 0) {
        return;
    }

    reach_windows_icon_destroy(reinterpret_cast<reach_windows_icon *>(icon_id));
}
