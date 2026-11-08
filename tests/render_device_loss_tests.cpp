#include "reach/support/render_device_loss.h"

#include <stdio.h>

static int failed = 0;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failed += 1;
    }
}

static void test_success_codes_are_ok(void)
{
    expect(reach_render_classify_hresult(0) == REACH_RENDER_HRESULT_OK, "S_OK is ok");
    expect(reach_render_classify_hresult(1) == REACH_RENDER_HRESULT_OK, "S_FALSE is ok");
}

static void test_device_loss_codes(void)
{
    expect(reach_render_classify_hresult((int32_t)0x887A0005u) == REACH_RENDER_HRESULT_DEVICE_LOST,
           "DXGI_ERROR_DEVICE_REMOVED is device lost");
    expect(reach_render_classify_hresult((int32_t)0x887A0006u) == REACH_RENDER_HRESULT_DEVICE_LOST,
           "DXGI_ERROR_DEVICE_HUNG is device lost");
    expect(reach_render_classify_hresult((int32_t)0x887A0007u) == REACH_RENDER_HRESULT_DEVICE_LOST,
           "DXGI_ERROR_DEVICE_RESET is device lost");
    expect(reach_render_classify_hresult((int32_t)0x887A0020u) == REACH_RENDER_HRESULT_DEVICE_LOST,
           "DXGI_ERROR_DRIVER_INTERNAL_ERROR is device lost");
    expect(reach_render_classify_hresult((int32_t)0x8899000Cu) == REACH_RENDER_HRESULT_DEVICE_LOST,
           "D2DERR_RECREATE_TARGET is device lost");
}

static void test_other_failures(void)
{
    expect(reach_render_classify_hresult((int32_t)0x80004005u) == REACH_RENDER_HRESULT_FAILURE,
           "E_FAIL is a failure");
    expect(reach_render_classify_hresult((int32_t)0x80070057u) == REACH_RENDER_HRESULT_FAILURE,
           "E_INVALIDARG is a failure");
    expect(reach_render_classify_hresult((int32_t)0x8007000Eu) == REACH_RENDER_HRESULT_FAILURE,
           "E_OUTOFMEMORY is a failure");
    expect(reach_render_classify_hresult((int32_t)0x887A0001u) == REACH_RENDER_HRESULT_FAILURE,
           "DXGI_ERROR_INVALID_CALL is a failure");
    expect(reach_render_classify_hresult((int32_t)0x887A0004u) == REACH_RENDER_HRESULT_FAILURE,
           "DXGI_ERROR_UNSUPPORTED is a failure");
}

int main(void)
{
    test_success_codes_are_ok();
    test_device_loss_codes();
    test_other_failures();
    return failed == 0 ? 0 : 1;
}
