#include "windows_adapters_internal.h"

#include "windows_icon_handle_internal.h"

#include <windows.h>

#include <wincodec.h>

#include <new>

struct reach_image_loader
{
    IWICImagingFactory *factory;
};

static void reach_image_loader_scaled_size(UINT source_width, UINT source_height,
                                           int32_t target_width, int32_t target_height,
                                           UINT *out_width, UINT *out_height)
{
    *out_width = source_width;
    *out_height = source_height;
    if (target_width <= 0 || target_height <= 0 || source_width == 0 || source_height == 0)
    {
        return;
    }

    double scale_x = (double)target_width / (double)source_width;
    double scale_y = (double)target_height / (double)source_height;
    double scale = scale_x > scale_y ? scale_x : scale_y;
    if (scale >= 1.0)
    {
        return;
    }

    UINT width = (UINT)((double)source_width * scale + 0.5);
    UINT height = (UINT)((double)source_height * scale + 0.5);
    *out_width = width > 0 ? width : 1;
    *out_height = height > 0 ? height : 1;
}

static reach_result reach_image_loader_load(reach_image_loader *loader, const uint16_t *path,
                                            int32_t target_width, int32_t target_height,
                                            uint64_t *out_image_id)
{
    if (loader == nullptr || loader->factory == nullptr || path == nullptr || path[0] == 0 ||
        out_image_id == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_image_id = 0;

    IWICBitmapDecoder *decoder = nullptr;
    if (FAILED(loader->factory->CreateDecoderFromFilename(reinterpret_cast<LPCWSTR>(path), nullptr,
                                                          GENERIC_READ,
                                                          WICDecodeMetadataCacheOnLoad, &decoder)) ||
        decoder == nullptr)
    {
        return REACH_ERROR;
    }

    IWICBitmapFrameDecode *frame = nullptr;
    if (FAILED(decoder->GetFrame(0, &frame)) || frame == nullptr)
    {
        decoder->Release();
        return REACH_ERROR;
    }

    UINT source_width = 0;
    UINT source_height = 0;
    if (FAILED(frame->GetSize(&source_width, &source_height)) || source_width == 0 ||
        source_height == 0)
    {
        frame->Release();
        decoder->Release();
        return REACH_ERROR;
    }

    UINT width = 0;
    UINT height = 0;
    reach_image_loader_scaled_size(source_width, source_height, target_width, target_height, &width,
                                   &height);

    IWICBitmapSource *source = nullptr;
    IWICBitmapScaler *scaler = nullptr;
    if (width != source_width || height != source_height)
    {
        if (SUCCEEDED(loader->factory->CreateBitmapScaler(&scaler)) && scaler != nullptr &&
            SUCCEEDED(scaler->Initialize(frame, width, height, WICBitmapInterpolationModeFant)))
        {
            source = scaler;
        }
    }
    if (source == nullptr)
    {
        width = source_width;
        height = source_height;
        source = frame;
    }

    IWICFormatConverter *converter = nullptr;
    if (FAILED(loader->factory->CreateFormatConverter(&converter)) || converter == nullptr ||
        FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeMedianCut)))
    {
        if (converter != nullptr)
        {
            converter->Release();
        }
        if (scaler != nullptr)
        {
            scaler->Release();
        }
        frame->Release();
        decoder->Release();
        return REACH_ERROR;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = (LONG)width;
    info.bmiHeader.biHeight = -(LONG)height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    reach_result result = REACH_ERROR;
    if (bitmap != nullptr && bits != nullptr &&
        SUCCEEDED(converter->CopyPixels(nullptr, width * 4, width * height * 4,
                                        static_cast<BYTE *>(bits))))
    {
        uint64_t image_id = reach_windows_icon_id_from_hbitmap(bitmap);
        if (image_id != 0)
        {
            *out_image_id = image_id;
            result = REACH_OK;
        }
    }

    if (result != REACH_OK && bitmap != nullptr)
    {
        DeleteObject(bitmap);
    }

    converter->Release();
    if (scaler != nullptr)
    {
        scaler->Release();
    }
    frame->Release();
    decoder->Release();
    return result;
}

static void reach_image_loader_release(reach_image_loader *loader, uint64_t image_id)
{
    (void)loader;
    if (image_id != 0)
    {
        reach_windows_icon_id_release(image_id);
    }
}

static void reach_image_loader_destroy(reach_image_loader *loader)
{
    if (loader == nullptr)
    {
        return;
    }
    if (loader->factory != nullptr)
    {
        loader->factory->Release();
        loader->factory = nullptr;
    }
    delete loader;
}

reach_result reach_windows_create_image_loader(reach_image_loader_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_image_loader *loader = new (std::nothrow) reach_image_loader();
    if (loader == nullptr)
    {
        return REACH_ERROR;
    }

    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&loader->factory))) ||
        loader->factory == nullptr)
    {
        delete loader;
        return REACH_ERROR;
    }

    out_port->loader = loader;
    out_port->ops.load = reach_image_loader_load;
    out_port->ops.release = reach_image_loader_release;
    out_port->ops.destroy = reach_image_loader_destroy;
    return REACH_OK;
}
