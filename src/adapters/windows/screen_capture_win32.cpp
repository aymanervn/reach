#include "windows_adapters_internal.h"

#include "reach/ports/screen_capture.h"

#include "windows_icon_handle_internal.h"

#include <windows.h>

#include <math.h>
#include <new>

struct reach_screen_capture
{
    int32_t unused;
};

static reach_result reach_screen_capture_capture(reach_screen_capture *capture,
                                                 reach_rect_f32 screen_bounds,
                                                 reach_screen_image *out_image)
{
    if (capture == nullptr || out_image == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_image = {};
    int32_t left = (int32_t)lroundf(screen_bounds.x);
    int32_t top = (int32_t)lroundf(screen_bounds.y);
    int32_t width = (int32_t)lroundf(screen_bounds.width);
    int32_t height = (int32_t)lroundf(screen_bounds.height);
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
    {
        return REACH_INVALID_ARGUMENT;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    if (screen == nullptr)
    {
        return REACH_ERROR;
    }
    HDC memory = CreateCompatibleDC(screen);
    void *bits = nullptr;
    HBITMAP bitmap = memory != nullptr
                         ? CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0)
                         : nullptr;
    reach_result result = REACH_ERROR;
    if (bitmap != nullptr && bits != nullptr)
    {
        HGDIOBJ previous = SelectObject(memory, bitmap);
        BOOL copied = BitBlt(memory, 0, 0, width, height, screen, left, top, SRCCOPY);
        SelectObject(memory, previous);
        GdiFlush();
        if (copied)
        {
            uint8_t *pixels = static_cast<uint8_t *>(bits);
            size_t stride = (size_t)width * 4u;
            size_t total = stride * (size_t)height;
            for (size_t alpha = 3; alpha < total; alpha += 4)
            {
                pixels[alpha] = 0xFF;
            }
            uint64_t image_id = reach_windows_icon_id_from_hbitmap(bitmap);
            if (image_id != 0)
            {
                out_image->image_id = image_id;
                out_image->screen_bounds = {(float)left, (float)top, (float)width, (float)height};
                out_image->pixels = {(uint32_t)width, (uint32_t)height, stride, pixels};
                bitmap = nullptr;
                result = REACH_OK;
            }
        }
    }
    if (bitmap != nullptr)
    {
        DeleteObject(bitmap);
    }
    if (memory != nullptr)
    {
        DeleteDC(memory);
    }
    ReleaseDC(nullptr, screen);
    return result;
}

static void reach_screen_capture_release(reach_screen_capture *capture, uint64_t image_id)
{
    (void)capture;
    reach_windows_icon_id_release(image_id);
}

static void reach_screen_capture_destroy(reach_screen_capture *capture)
{
    delete capture;
}

extern "C" reach_result reach_windows_create_screen_capture(reach_screen_capture_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_port = {};
    reach_screen_capture *capture = new (std::nothrow) reach_screen_capture();
    if (capture == nullptr)
    {
        return REACH_ERROR;
    }
    out_port->capture = capture;
    out_port->ops.capture = reach_screen_capture_capture;
    out_port->ops.release = reach_screen_capture_release;
    out_port->ops.destroy = reach_screen_capture_destroy;
    return REACH_OK;
}
