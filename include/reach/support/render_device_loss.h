#ifndef REACH_RENDER_DEVICE_LOSS_H
#define REACH_RENDER_DEVICE_LOSS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_render_hresult_class
    {
        REACH_RENDER_HRESULT_OK = 0,
        REACH_RENDER_HRESULT_DEVICE_LOST = 1,
        REACH_RENDER_HRESULT_FAILURE = 2
    } reach_render_hresult_class;

    reach_render_hresult_class reach_render_classify_hresult(int32_t hr);
    int32_t reach_render_hardware_unavailable_hresult(int32_t hr);

#ifdef __cplusplus
}
#endif

#endif
