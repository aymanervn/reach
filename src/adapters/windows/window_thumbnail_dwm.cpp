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
    HWND destination;
    reach_window_thumbnail_plane plane;
    COLORREF background;
    int32_t background_set;
    BYTE background_opacity;
    int32_t opacity_set;
} reach_window_thumbnail_entry;

struct reach_window_thumbnails
{
    HWND target;
    reach_window_thumbnail_id next_id;
    reach_window_thumbnail_entry entries[REACH_WINDOW_THUMBNAIL_MAX];
    size_t entry_count;
};

static const wchar_t *reach_window_thumbnail_host_class(void)
{
    return L"ReachWindowThumbnailHost";
}

static LRESULT CALLBACK reach_window_thumbnail_host_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                         LPARAM lparam)
{
    if (message == WM_ERASEBKGND)
    {
        return 1;
    }
    if (message == WM_PAINT)
    {
        reach_window_thumbnail_entry *entry = reinterpret_cast<reach_window_thumbnail_entry *>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        COLORREF color = entry != nullptr && entry->background_set ? entry->background
                                                                   : RGB(0, 0, 0);
        PAINTSTRUCT paint = {};
        HDC dc = BeginPaint(hwnd, &paint);
        if (dc != nullptr)
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            HBRUSH brush = CreateSolidBrush(color);
            if (brush != nullptr)
            {
                FillRect(dc, &client, brush);
                DeleteObject(brush);
            }
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    if (message == WM_NCHITTEST)
    {
        return HTTRANSPARENT;
    }
    if (message == WM_MOUSEACTIVATE)
    {
        return MA_NOACTIVATE;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static reach_result reach_window_thumbnail_register_host_class(void)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_window_thumbnail_host_proc;
    wc.hInstance = instance;
    wc.lpszClassName = reach_window_thumbnail_host_class();
    ATOM atom = RegisterClassExW(&wc);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS ? REACH_OK : REACH_ERROR;
}

static HWND reach_window_thumbnail_create_host(HWND target)
{
    DWORD topmost = (GetWindowLongPtrW(target, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0
                        ? WS_EX_TOPMOST : 0;
    return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | topmost,
                           reach_window_thumbnail_host_class(), L"", WS_POPUP, 0, 0, 1, 1, nullptr,
                           nullptr, GetModuleHandleW(nullptr), nullptr);
}

static BYTE reach_window_thumbnail_color_channel(float value)
{
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    else if (value > 1.0f)
    {
        value = 1.0f;
    }
    return (BYTE)(value * 255.0f + 0.5f);
}

static COLORREF reach_window_thumbnail_color(reach_color color)
{
    return RGB(reach_window_thumbnail_color_channel(color.r),
               reach_window_thumbnail_color_channel(color.g),
               reach_window_thumbnail_color_channel(color.b));
}

static void reach_window_thumbnail_release_entry(reach_window_thumbnail_entry *entry)
{
    if (entry == nullptr)
    {
        return;
    }
    if (entry->handle != nullptr)
    {
        DwmUnregisterThumbnail(entry->handle);
    }
    if (entry->plane == REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET &&
        entry->destination != nullptr)
    {
        SetWindowLongPtrW(entry->destination, GWLP_USERDATA, 0);
        DestroyWindow(entry->destination);
    }
    *entry = {};
}

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
        reach_window_thumbnail_release_entry(&thumbnails->entries[index]);
    }
    thumbnails->entry_count = 0;
    thumbnails->target = hwnd;
    return REACH_OK;
}

static reach_result reach_window_thumbnail_create(reach_window_thumbnails *thumbnails,
                                                  reach_window_id source,
                                                  reach_window_thumbnail_plane plane,
                                                  reach_window_thumbnail_id *out_id)
{
    if (thumbnails == nullptr || out_id == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_id = REACH_WINDOW_THUMBNAIL_NONE;

    if (thumbnails->target == nullptr || thumbnails->entry_count >= REACH_WINDOW_THUMBNAIL_MAX ||
        (plane != REACH_WINDOW_THUMBNAIL_PLANE_TARGET &&
         plane != REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET))
    {
        return REACH_ERROR;
    }

    HWND source_hwnd = reinterpret_cast<HWND>(source);
    if (source_hwnd == nullptr || source_hwnd == thumbnails->target || !IsWindow(source_hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND destination = thumbnails->target;
    if (plane == REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET)
    {
        destination = reach_window_thumbnail_create_host(thumbnails->target);
        if (destination == nullptr)
        {
            return REACH_ERROR;
        }
    }

    HTHUMBNAIL handle = nullptr;
    if (FAILED(DwmRegisterThumbnail(destination, source_hwnd, &handle)) || handle == nullptr)
    {
        if (destination != thumbnails->target)
        {
            DestroyWindow(destination);
        }
        return REACH_ERROR;
    }

    reach_window_thumbnail_entry *entry = &thumbnails->entries[thumbnails->entry_count];
    entry->id = ++thumbnails->next_id;
    entry->handle = handle;
    entry->source = source_hwnd;
    entry->destination = destination;
    entry->plane = plane;
    if (plane == REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET)
    {
        SetWindowLongPtrW(destination, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(entry));
    }
    thumbnails->entry_count++;

    *out_id = entry->id;
    return REACH_OK;
}

static reach_result
reach_window_thumbnail_set_placement(reach_window_thumbnails *thumbnails,
                                     reach_window_thumbnail_id id,
                                     const reach_window_thumbnail_placement *placement)
{
    reach_window_thumbnail_entry *entry = reach_window_thumbnail_find(thumbnails, id);
    if (entry == nullptr || placement == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    float clamped = placement->opacity;
    if (clamped < 0.0f)
    {
        clamped = 0.0f;
    }
    else if (clamped > 1.0f)
    {
        clamped = 1.0f;
    }

    DWM_THUMBNAIL_PROPERTIES props = {};
    props.dwFlags =
        DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;

    LONG destination_left = (LONG)(placement->destination.x + 0.5f);
    LONG destination_top = (LONG)(placement->destination.y + 0.5f);
    LONG destination_right =
        (LONG)(placement->destination.x + placement->destination.width + 0.5f);
    LONG destination_bottom =
        (LONG)(placement->destination.y + placement->destination.height + 0.5f);

    if (entry->plane == REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET)
    {
        RECT target_rect = {};
        if (entry->destination == nullptr || thumbnails->target == nullptr ||
            !GetWindowRect(thumbnails->target, &target_rect))
        {
            return REACH_ERROR;
        }

        LONG target_width = target_rect.right - target_rect.left;
        LONG target_height = target_rect.bottom - target_rect.top;
        LONG thumbnail_width = destination_right - destination_left;
        LONG thumbnail_height = destination_bottom - destination_top;
        int32_t thumbnail_visible =
            placement->visible && thumbnail_width > 0 && thumbnail_height > 0;
        int32_t host_visible =
            (placement->background_visible || thumbnail_visible) && target_width > 0 &&
            target_height > 0;

        COLORREF background = reach_window_thumbnail_color(placement->background);
        int32_t background_changed = !entry->background_set || entry->background != background;
        if (background_changed)
        {
            entry->background = background;
            entry->background_set = 1;
        }

        BYTE opacity = reach_window_thumbnail_color_channel(placement->background.a);
        if (!entry->opacity_set || entry->background_opacity != opacity)
        {
            if (!SetLayeredWindowAttributes(entry->destination, 0, opacity, LWA_ALPHA))
            {
                return REACH_ERROR;
            }
            entry->background_opacity = opacity;
            entry->opacity_set = 1;
        }
        bool was_visible = IsWindowVisible(entry->destination) != 0;
        if (host_visible)
        {
            RECT current = {};
            bool bounds_changed = !GetWindowRect(entry->destination, &current) ||
                                  !EqualRect(&current, &target_rect);
            bool order_changed = GetWindow(entry->destination, GW_HWNDPREV) != thumbnails->target;
            if (bounds_changed || order_changed || !was_visible)
            {
                UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER;
                if (!bounds_changed)
                {
                    flags |= SWP_NOMOVE | SWP_NOSIZE;
                }
                if (!order_changed)
                {
                    flags |= SWP_NOZORDER;
                }
                if (!was_visible)
                {
                    flags |= SWP_SHOWWINDOW;
                }
                if (!SetWindowPos(entry->destination, thumbnails->target, target_rect.left,
                                  target_rect.top, target_width, target_height, flags))
                {
                    return REACH_ERROR;
                }
            }
            if (background_changed || bounds_changed || !was_visible)
            {
                InvalidateRect(entry->destination, nullptr, FALSE);
                UpdateWindow(entry->destination);
            }
        }
        else if (was_visible)
        {
            ShowWindow(entry->destination, SW_HIDE);
        }

        props.rcDestination.left = destination_left;
        props.rcDestination.top = destination_top;
        props.rcDestination.right = destination_right;
        props.rcDestination.bottom = destination_bottom;
        props.fVisible = thumbnail_visible ? TRUE : FALSE;
    }
    else
    {
        props.rcDestination.left = destination_left;
        props.rcDestination.top = destination_top;
        props.rcDestination.right = destination_right;
        props.rcDestination.bottom = destination_bottom;
    }

    RECT window_rect = {};
    if (placement->source_screen_valid)
    {
        if (entry->source == nullptr || !GetWindowRect(entry->source, &window_rect) ||
            placement->source_screen.width <= 0.0f || placement->source_screen.height <= 0.0f)
        {
            return REACH_INVALID_ARGUMENT;
        }

        RECT crop = {};
        crop.left = (LONG)placement->source_screen.x - window_rect.left;
        crop.top = (LONG)placement->source_screen.y - window_rect.top;
        crop.right =
            (LONG)(placement->source_screen.x + placement->source_screen.width) - window_rect.left;
        crop.bottom =
            (LONG)(placement->source_screen.y + placement->source_screen.height) - window_rect.top;

        if (crop.left < 0 || crop.top < 0 || crop.right <= crop.left || crop.bottom <= crop.top ||
            crop.right > window_rect.right - window_rect.left ||
            crop.bottom > window_rect.bottom - window_rect.top)
        {
            return REACH_INVALID_ARGUMENT;
        }

        props.dwFlags |= DWM_TNP_RECTSOURCE;
        props.rcSource = crop;
    }
    else
    {
        RECT visible_rect = {};
        if (entry->source != nullptr && GetWindowRect(entry->source, &window_rect) &&
            SUCCEEDED(DwmGetWindowAttribute(entry->source, DWMWA_EXTENDED_FRAME_BOUNDS,
                                            &visible_rect, sizeof(visible_rect))))
        {
            RECT crop = {};
            crop.left = visible_rect.left - window_rect.left;
            crop.top = visible_rect.top - window_rect.top;
            crop.right = visible_rect.right - window_rect.left;
            crop.bottom = visible_rect.bottom - window_rect.top;

            if (crop.left >= 0 && crop.top >= 0 && crop.right > crop.left &&
                crop.bottom > crop.top && crop.right <= window_rect.right - window_rect.left &&
                crop.bottom <= window_rect.bottom - window_rect.top)
            {
                props.dwFlags |= DWM_TNP_RECTSOURCE;
                props.rcSource = crop;
            }
        }
    }

    if (entry->plane == REACH_WINDOW_THUMBNAIL_PLANE_TARGET)
    {
        props.fVisible = placement->visible != 0 ? TRUE : FALSE;
    }
    props.opacity = (BYTE)(clamped * 255.0f + 0.5f);
    props.fSourceClientAreaOnly = FALSE;

    return SUCCEEDED(DwmUpdateThumbnailProperties(entry->handle, &props)) ? REACH_OK : REACH_ERROR;
}

static reach_window_id reach_window_thumbnail_cover_window(const reach_window_thumbnails *thumbnails)
{
    if (thumbnails != nullptr)
    {
        for (size_t index = 0; index < thumbnails->entry_count; ++index)
        {
            const reach_window_thumbnail_entry *entry = &thumbnails->entries[index];
            if (entry->plane == REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET &&
                entry->opacity_set && entry->background_opacity == 255 &&
                IsWindowVisible(entry->destination))
            {
                return reinterpret_cast<reach_window_id>(entry->destination);
            }
        }
    }
    return 0;
}

static reach_result reach_window_thumbnail_destroy_all(reach_window_thumbnails *thumbnails)
{
    if (thumbnails == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < thumbnails->entry_count; ++index)
    {
        reach_window_thumbnail_release_entry(&thumbnails->entries[index]);
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
    if (reach_window_thumbnail_register_host_class() != REACH_OK)
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
    out_port->ops.cover_window = reach_window_thumbnail_cover_window;
    return REACH_OK;
}
