#include "render_d2d_internal.h"
#include "reach/support/workload.h"

#include <algorithm>
#include <math.h>

// Workaround: dwmapi ordinal 147 (DwmpCreateSharedThumbnailVisual) is undocumented. It returns a
// live thumbnail of another window as a DirectComposition visual, so the compositor can animate it
// with the rest of the surface instead of the CPU moving a classic thumbnail every frame. Its
// properties are fixed at creation (DwmUpdateThumbnailProperties rejects the handle), so a change
// of size or crop recreates it. Probed on build 26100 (2026-11-23).
typedef HRESULT(WINAPI *reach_d2d_shared_thumbnail_fn)(HWND destination, HWND source, DWORD flags,
                                                       DWM_THUMBNAIL_PROPERTIES *properties,
                                                       void *dcomp_device, void **out_visual,
                                                       PHTHUMBNAIL out_handle);

static reach_d2d_shared_thumbnail_fn reach_d2d_shared_thumbnail(void)
{
    static reach_d2d_shared_thumbnail_fn function = []
    {
        HMODULE module = GetModuleHandleW(L"dwmapi.dll");
        if (module == nullptr)
            module = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        return module != nullptr ? reinterpret_cast<reach_d2d_shared_thumbnail_fn>(
                                       GetProcAddress(module, MAKEINTRESOURCEA(147)))
                                 : nullptr;
    }();
    return function;
}

reach_d2d_window_thumbnail::~reach_d2d_window_thumbnail()
{
    if (parent != nullptr && visual != nullptr)
    {
        reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
        (void)parent->RemoveVisual(visual.Get());
    }
    visual.Reset();
    if (handle != nullptr)
    {
        reach_workload_add(REACH_WORKLOAD_THUMBNAIL_RELEASES, 1);
        (void)DwmUnregisterThumbnail(handle);
    }
}

int32_t reach_d2d_window_thumbnails_supported(const reach_render_backend *backend)
{
    return backend != nullptr && backend->compositor == nullptr &&
           backend->dcomp_device != nullptr && reach_d2d_shared_thumbnail() != nullptr;
}

static bool reach_d2d_thumbnail_source(HWND source, const reach_render_command &command,
                                       RECT *out_crop)
{
    RECT window = {};
    RECT visible = {};
    if (IsIconic(source))
    {
        // A minimized window gets the crop it will have once restored, so restoring it keeps the
        // same thumbnail instead of recreating it mid-flight.
        reach_rect_f32 restored_window = {};
        reach_rect_f32 restored_frame = {};
        if (!reach_windows_restored_bounds((uintptr_t)source, &restored_window, &restored_frame))
            return false;
        window = {(LONG)restored_window.x, (LONG)restored_window.y,
                  (LONG)(restored_window.x + restored_window.width),
                  (LONG)(restored_window.y + restored_window.height)};
        visible = {(LONG)restored_frame.x, (LONG)restored_frame.y,
                   (LONG)(restored_frame.x + restored_frame.width),
                   (LONG)(restored_frame.y + restored_frame.height)};
    }
    else if (!GetWindowRect(source, &window))
        return false;
    else if (command.source_screen.width > 0.0f && command.source_screen.height > 0.0f)
    {
        visible.left = (LONG)command.source_screen.x;
        visible.top = (LONG)command.source_screen.y;
        visible.right = (LONG)(command.source_screen.x + command.source_screen.width);
        visible.bottom = (LONG)(command.source_screen.y + command.source_screen.height);
    }
    else if (FAILED(DwmGetWindowAttribute(source, DWMWA_EXTENDED_FRAME_BOUNDS, &visible,
                                          sizeof(visible))))
        return false;
    RECT crop = {visible.left - window.left, visible.top - window.top, visible.right - window.left,
                 visible.bottom - window.top};
    if (crop.left < 0 || crop.top < 0 || crop.right <= crop.left || crop.bottom <= crop.top ||
        crop.right > window.right - window.left || crop.bottom > window.bottom - window.top)
        return false;
    *out_crop = crop;
    return true;
}

HRESULT reach_d2d_sync_window_thumbnail(reach_render_backend *backend,
                                        reach_d2d_retained_layer &layer,
                                        const reach_render_command &command)
{
    reach_d2d_shared_thumbnail_fn create = reach_d2d_shared_thumbnail();
    HWND source = reinterpret_cast<HWND>((uintptr_t)command.window_id);
    if (!reach_d2d_window_thumbnails_supported(backend) || layer.dcomp_visual == nullptr)
        return S_OK;
    HRESULT hr = layer.dcomp_visual->SetContent(nullptr);
    if (FAILED(hr))
        return hr;
    UINT width = (UINT)std::max(1.0f, ceilf(command.rect.width));
    UINT height = (UINT)std::max(1.0f, ceilf(command.rect.height));
    RECT crop = {};
    bool has_crop = IsWindow(source) && reach_d2d_thumbnail_source(source, command, &crop);
    auto *current = layer.thumbnail.get();
    bool reusable = current != nullptr && current->window == command.window_id &&
                    current->width == width && current->height == height &&
                    current->has_source == has_crop &&
                    (!has_crop || EqualRect(&current->source, &crop));
    if (!reusable)
    {
        layer.thumbnail.reset();
        if (!IsWindow(source))
            return S_OK;
        DWM_THUMBNAIL_PROPERTIES properties = {};
        properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY |
                             DWM_TNP_SOURCECLIENTAREAONLY;
        properties.rcDestination = {0, 0, (LONG)width, (LONG)height};
        properties.fVisible = TRUE;
        properties.opacity = 255;
        properties.fSourceClientAreaOnly = FALSE;
        if (has_crop)
        {
            properties.dwFlags |= DWM_TNP_RECTSOURCE;
            properties.rcSource = crop;
        }
        auto thumbnail = std::make_shared<reach_d2d_window_thumbnail>();
        IUnknown *visual = nullptr;
        reach_workload_add(REACH_WORKLOAD_THUMBNAIL_REGISTRATIONS, 1);
        hr = create(backend->hwnd, source, 0, &properties, backend->dcomp_device,
                    reinterpret_cast<void **>(&visual), &thumbnail->handle);
        if (SUCCEEDED(hr) && visual == nullptr)
            hr = E_POINTER;
        if (SUCCEEDED(hr))
            hr = visual->QueryInterface(IID_PPV_ARGS(&thumbnail->visual));
        if (visual != nullptr)
            visual->Release();
        if (FAILED(hr))
        {
            // Thumbnails are best-effort: a window that refuses one leaves its tile empty instead
            // of failing the whole surface.
            reach_d2d_log_hresult(L"DwmpCreateSharedThumbnailVisual", hr);
            return S_OK;
        }
        reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
        hr = layer.dcomp_visual->AddVisual(thumbnail->visual.Get(), FALSE, nullptr);
        if (FAILED(hr))
            return hr;
        thumbnail->parent = layer.dcomp_visual;
        thumbnail->window = command.window_id;
        thumbnail->width = width;
        thumbnail->height = height;
        thumbnail->source = crop;
        thumbnail->has_source = has_crop;
        layer.thumbnail = std::move(thumbnail);
    }
    hr = layer.thumbnail->visual->SetOffsetX(command.rect.x);
    if (SUCCEEDED(hr))
        hr = layer.thumbnail->visual->SetOffsetY(command.rect.y);
    return hr;
}
