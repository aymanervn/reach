#ifndef REACH_CORE_IMAGE_H
#define REACH_CORE_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_image_view
    {
        uint32_t width;
        uint32_t height;
        size_t stride;
        const uint8_t *bgra_pixels;
    } reach_image_view;

    static inline reach_image_view reach_image_view_crop(reach_image_view image, uint32_t x,
                                                         uint32_t y, uint32_t width,
                                                         uint32_t height)
    {
        reach_image_view cropped = {0, 0, image.stride, NULL};
        if (image.bgra_pixels == NULL || x >= image.width || y >= image.height || width == 0 ||
            height == 0)
        {
            return cropped;
        }
        cropped.width = width < image.width - x ? width : image.width - x;
        cropped.height = height < image.height - y ? height : image.height - y;
        cropped.bgra_pixels = image.bgra_pixels + (size_t)y * image.stride + (size_t)x * 4u;
        return cropped;
    }

#ifdef __cplusplus
}
#endif

#endif
