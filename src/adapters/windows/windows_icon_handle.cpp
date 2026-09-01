#include "windows_icon_handle_internal.h"

#include <mutex>
#include <new>
#include <vector>

static std::mutex &reach_windows_icon_lock(void)
{
    static std::mutex lock;
    return lock;
}

static std::vector<reach_windows_icon *> &reach_windows_icon_registry(void)
{
    static std::vector<reach_windows_icon *> registry;
    return registry;
}

static reach_windows_icon *reach_windows_icon_find(uint64_t icon_id)
{
    for (reach_windows_icon *icon : reach_windows_icon_registry())
    {
        if (icon->id == icon_id)
        {
            return icon;
        }
    }

    return nullptr;
}

static void reach_windows_icon_destroy_handles(HICON hicon, HBITMAP hbitmap)
{
    if (hicon != nullptr)
    {
        DestroyIcon(hicon);
    }

    if (hbitmap != nullptr)
    {
        DeleteObject(hbitmap);
    }
}

static void reach_windows_icon_destroy(reach_windows_icon *icon)
{
    if (icon == nullptr)
    {
        return;
    }

    reach_windows_icon_destroy_handles(icon->hicon, icon->hbitmap);
    delete icon;
}

static uint64_t reach_windows_icon_register(reach_windows_icon_kind kind, HICON hicon,
                                            HBITMAP hbitmap)
{
    reach_windows_icon *icon = new (std::nothrow) reach_windows_icon();
    if (icon == nullptr)
    {
        reach_windows_icon_destroy_handles(hicon, hbitmap);
        return 0;
    }

    icon->references = 1;
    icon->kind = kind;
    icon->hicon = hicon;
    icon->hbitmap = hbitmap;

    {
        std::lock_guard<std::mutex> guard(reach_windows_icon_lock());

        // Never reused, so a released id resolves to nothing instead of to whichever icon later
        // takes its place. Render backends cache bitmaps by id and would otherwise serve a dead
        // icon's bitmap for a live one.
        static uint64_t next_id = 0;
        icon->id = ++next_id;

        try
        {
            reach_windows_icon_registry().push_back(icon);
        }
        catch (...)
        {
            reach_windows_icon_destroy(icon);
            return 0;
        }
    }

    return icon->id;
}

uint64_t reach_windows_icon_id_from_hicon(HICON hicon)
{
    if (hicon == nullptr)
    {
        return 0;
    }

    return reach_windows_icon_register(REACH_WINDOWS_ICON_KIND_HICON, hicon, nullptr);
}

uint64_t reach_windows_icon_id_from_hbitmap(HBITMAP hbitmap)
{
    if (hbitmap == nullptr)
    {
        return 0;
    }

    return reach_windows_icon_register(REACH_WINDOWS_ICON_KIND_HBITMAP, nullptr, hbitmap);
}

reach_windows_icon *reach_windows_icon_lookup(uint64_t icon_id)
{
    if (icon_id == 0)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> guard(reach_windows_icon_lock());
    return reach_windows_icon_find(icon_id);
}

void reach_windows_icon_id_retain(uint64_t icon_id)
{
    if (icon_id == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(reach_windows_icon_lock());
    reach_windows_icon *icon = reach_windows_icon_find(icon_id);
    if (icon != nullptr)
    {
        ++icon->references;
    }
}

void reach_windows_icon_id_release(uint64_t icon_id)
{
    if (icon_id == 0)
    {
        return;
    }

    reach_windows_icon *retired = nullptr;
    {
        std::lock_guard<std::mutex> guard(reach_windows_icon_lock());
        std::vector<reach_windows_icon *> &registry = reach_windows_icon_registry();
        for (size_t index = 0; index < registry.size(); ++index)
        {
            if (registry[index]->id != icon_id)
            {
                continue;
            }

            if (--registry[index]->references > 0)
            {
                return;
            }

            retired = registry[index];
            registry.erase(registry.begin() + index);
            break;
        }
    }

    reach_windows_icon_destroy(retired);
}
