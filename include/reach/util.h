#ifndef REACH_UTIL_H
#define REACH_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum reach_result {
    REACH_OK = 0,
    REACH_ERROR = 1,
    REACH_INVALID_ARGUMENT = 2,
    REACH_NOT_IMPLEMENTED = 3
} reach_result;

typedef struct reach_rect_i32 {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} reach_rect_i32;

typedef struct reach_vec2 {
    float x;
    float y;
} reach_vec2;

#define REACH_HR_SUCCEEDED(hr) ((long)(hr) >= 0)
#define REACH_HR_FAILED(hr) ((long)(hr) < 0)
#define REACH_ASSERT(expr) assert(expr)

void reach_log_info(const char *message);
void reach_log_error(const char *message);
size_t reach_strlen_utf16(const uint16_t *text);
reach_result reach_copy_utf16(uint16_t *dst, size_t dst_count, const uint16_t *src);

#ifdef __cplusplus
}
#endif

#endif
