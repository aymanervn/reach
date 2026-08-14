#include "windows_adapters_internal.h"

#include "reach/core/limits.h"

#include <windows.h>

#include <dwmapi.h>

#include <new>

#define REACH_WINDOW_THUMBNAIL_MAX (REACH_MAX_OPEN_WINDOWS + 1)

typedef struct reach_window_thumbnail_entry
{
    reach_window_thumbnail_id id;
    HTHUMBNAIL handle;
    HWND source;
} reach_window_thumbnail_entry;

struct reach_window_thumbnails
{
    HWND target;
    reach_window_thumbnail_id next_id;
    reach_window_thumbnail_entry entries[REACH_WINDOW_THUMBNAIL_MAX];
    size_t entry_count;
};

static reach_window_thumbnail_entry *
reach_window_thumbnail_find(reach_window_thumbnails *thumbnails, reach_window_thumbnail_id id)
{
    if (thumbnails == nullptr || id == REACH_WINDOW_THUMBNAIL_NONE)
    {
        return nullptr;
    }

    for (size_t index = 0; index < thumbnails->entry_count; ++index)
    {
        if (thumbnails->entries[index].id == id)
        {
            return &thumbnails->entries[index];
        }
    }
    return nullptr;
}

static reach_result reach_window_thumbnail_set_target(reach_window_thumbnails *thumbnails,
                                                      reach_window_id target)
{
    if (thumbnails == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(target);
    if (thumbnails->target == hwnd)
    {
        return REACH_OK;
    }

    for (size_t index = 0; index < thumbnails->entry_count; ++index)
    {
        if (thumbnails->entries[index].handle != nullptr)
        {
            DwmUnregisterThumbnail(thumbnails->entries[index].handle);
        }
    }
    thumbnails->entry_count = 0;
    thumbnails->target = hwnd;
    return REACH_OK;
}

static reach_result reach_window_thumbnail_create(reach_window_thumbnails *thumbnails,
                                                  reach_window_id source,
                                                  reach_window_thumbnail_id *out_id)
{
    if (thumbnails == nullptr || out_id == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_id = REACH_WINDOW_THUMBNAIL_NONE;

    if (thumbnails->target == nullptr || thumbnails->entry_count >= REACH_WINDOW_THUMBNAIL_MAX)
    {
        return REACH_ERROR;
    }

    HWND source_hwnd = reinterpret_cast<HWND>(source);
    if (source_hwnd == nullptr || source_hwnd == thumbnails->target || !IsWindow(source_hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    HTHUMBNAIL handle = nullptr;
    if (FAILED(DwmRegisterThumbnail(thumbnails->target, source_hwnd, &handle)) || handle == nullptr)
    {
        return REACH_ERROR;
    }

    reach_window_thumbnail_entry *entry = &thumbnails->entries[thumbnails->entry_count];
    entry->id = ++thumbnails->next_id;
    entry->handle = handle;
    entry->source = source_hwnd;
    thumbnails->entry_count++;

    *out_id = entry->id;
    return REACH_OK;
}

static reach_result reach_window_thumbnail_set_placement(reach_window_thumbnails *thumbnails,
                                                         reach_window_thumbnail_id id,
                                                         reach_rect_f32 destination, float opacity,
                                                         int32_t visible)
{
    reach_window_thumbnail_entry *entry = reach_window_thumbnail_find(thumbnails, id);
    if (entry == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    float clamped = opacity;
    if (clamped < 0.0f)
    {
        clamped = 0.0f;
    }
    else if (clamped > 1.0f)
    {
        clamped = 1.0f;
    }

    DWM_THUMBNAIL_PROPERTIES props = {};
    props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY |
                    DWM_TNP_SOURCECLIENTAREAONLY;

    RECT window_rect = {};
    RECT visible_rect = {};
    if (entry->source != nullptr && GetWindowRect(entry->source, &window_rect) &&
        SUCCEEDED(DwmGetWindowAttribute(entry->source, DWMWA_EXTENDED_FRAME_BOUNDS, &visible_rect,
                                        sizeof(visible_rect))))
    {
        RECT crop = {};
        crop.left = visible_rect.left - window_rect.left;
        crop.top = visible_rect.top - window_rect.top;
        crop.right = visible_rect.right - window_rect.left;
        crop.bottom = visible_rect.bottom - window_rect.top;

        if (crop.left >= 0 && crop.top >= 0 && crop.right > crop.left && crop.bottom > crop.top &&
            crop.right <= window_rect.right - window_rect.left &&
            crop.bottom <= window_rect.bottom - window_rect.top)
        {
            props.dwFlags |= DWM_TNP_RECTSOURCE;
            props.rcSource = crop;
        }
    }

    props.rcDestination.left = (LONG)(destination.x + 0.5f);
    props.rcDestination.top = (LONG)(destination.y + 0.5f);
    props.rcDestination.right = (LONG)(destination.x + destination.width + 0.5f);
    props.rcDestination.bottom = (LONG)(destination.y + destination.height + 0.5f);
    props.fVisible = visible != 0 ? TRUE : FALSE;
    props.opacity = (BYTE)(clamped * 255.0f + 0.5f);
    props.fSourceClientAreaOnly = FALSE;

    return SUCCEEDED(DwmUpdateThumbnailProperties(entry->handle, &props)) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_window_thumbnail_destroy_all(reach_window_thumbnails *thumbnails)
{
    if (thumbnails == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < thumbnails->entry_count; ++index)
    {
        if (thumbnails->entries[index].handle != nullptr)
        {
            DwmUnregisterThumbnail(thumbnails->entries[index].handle);
        }
    }
    thumbnails->entry_count = 0;
    return REACH_OK;
}

static void reach_window_thumbnail_destroy(reach_window_thumbnails *thumbnails)
{
    if (thumbnails == nullptr)
    {
        return;
    }

    (void)reach_window_thumbnail_destroy_all(thumbnails);
    delete thumbnails;
}

reach_result reach_windows_create_window_thumbnails(reach_window_thumbnail_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    BOOL composition_enabled = FALSE;
    if (FAILED(DwmIsCompositionEnabled(&composition_enabled)) || !composition_enabled)
    {
        return REACH_ERROR;
    }

    reach_window_thumbnails *thumbnails = new (std::nothrow) reach_window_thumbnails();
    if (thumbnails == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->thumbnails = thumbnails;
    out_port->ops.set_target = reach_window_thumbnail_set_target;
    out_port->ops.create = reach_window_thumbnail_create;
    out_port->ops.set_placement = reach_window_thumbnail_set_placement;
    out_port->ops.destroy_all = reach_window_thumbnail_destroy_all;
    out_port->ops.destroy = reach_window_thumbnail_destroy;
    return REACH_OK;
}
