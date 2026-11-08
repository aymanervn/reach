#include "reach/support/render_device_loss.h"

static const uint32_t REACH_HRESULT_DXGI_DEVICE_REMOVED = 0x887A0005u;
static const uint32_t REACH_HRESULT_DXGI_DEVICE_HUNG = 0x887A0006u;
static const uint32_t REACH_HRESULT_DXGI_DEVICE_RESET = 0x887A0007u;
static const uint32_t REACH_HRESULT_DXGI_DRIVER_INTERNAL_ERROR = 0x887A0020u;
static const uint32_t REACH_HRESULT_D2D_RECREATE_TARGET = 0x8899000Cu;

reach_render_hresult_class reach_render_classify_hresult(int32_t hr)
{
    if (hr >= 0)
    {
        return REACH_RENDER_HRESULT_OK;
    }

    switch ((uint32_t)hr)
    {
    case REACH_HRESULT_DXGI_DEVICE_REMOVED:
    case REACH_HRESULT_DXGI_DEVICE_HUNG:
    case REACH_HRESULT_DXGI_DEVICE_RESET:
    case REACH_HRESULT_DXGI_DRIVER_INTERNAL_ERROR:
    case REACH_HRESULT_D2D_RECREATE_TARGET:
        return REACH_RENDER_HRESULT_DEVICE_LOST;
    default:
        return REACH_RENDER_HRESULT_FAILURE;
    }
}
