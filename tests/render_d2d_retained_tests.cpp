#include "render_d2d_retained_policy.h"

#include <limits>
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

static void test_layer_bytes(void)
{
    expect(reach_d2d_layer_bytes(100.0f, 50.0f, true, false) == 100u * 50u * 4u,
           "a painted layer costs four bytes per pixel");
    expect(reach_d2d_layer_bytes(100.2f, 50.5f, true, false) == 101u * 51u * 4u,
           "fractional bounds round up per axis");
    expect(reach_d2d_layer_bytes(100.0f, 50.0f, true, true) == 100u * 50u * 4u * 2u,
           "a painted layer with a shadow costs two surfaces");
    expect(reach_d2d_layer_bytes(100.0f, 50.0f, false, true) == 100u * 50u * 4u,
           "a shadow-only layer costs one surface");
    expect(reach_d2d_layer_bytes(100.0f, 50.0f, false, false) == 0,
           "a paint-less shadow-less layer costs nothing");
    expect(reach_d2d_layer_bytes(0.0f, 50.0f, true, true) == 0, "empty bounds cost nothing");
    expect(reach_d2d_layer_bytes(-4.0f, 50.0f, true, false) == 0, "negative bounds cost nothing");
    expect(reach_d2d_layer_bytes(std::numeric_limits<float>::quiet_NaN(), 50.0f, true, false) == 0,
           "NaN bounds cost nothing");
}

static void test_layer_bytes_saturates(void)
{
    expect(reach_d2d_layer_bytes(1e30f, 1e30f, true, true) > UINT64_C(128) * 1024 * 1024,
           "absurd bounds exceed the retained budget without overflowing");
    expect(reach_d2d_layer_bytes(std::numeric_limits<float>::infinity(), 1000.0f, true, false) >
               UINT64_C(128) * 1024 * 1024,
           "infinite bounds exceed the retained budget");
}

int main(void)
{
    test_layer_bytes();
    test_layer_bytes_saturates();
    return failed == 0 ? 0 : 1;
}
