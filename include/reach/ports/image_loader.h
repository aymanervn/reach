#ifndef REACH_PORTS_IMAGE_LOADER_H
#define REACH_PORTS_IMAGE_LOADER_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_image_loader reach_image_loader;

    typedef struct reach_image_loader_ops
    {
        reach_result (*load)(reach_image_loader *loader, const uint16_t *path, int32_t target_width,
                             int32_t target_height, uint64_t *out_image_id);

        void (*release)(reach_image_loader *loader, uint64_t image_id);

        void (*destroy)(reach_image_loader *loader);
    } reach_image_loader_ops;

    typedef struct reach_image_loader_port
    {
        reach_image_loader *loader;
        reach_image_loader_ops ops;
    } reach_image_loader_port;

#ifdef __cplusplus
}
#endif

#endif
