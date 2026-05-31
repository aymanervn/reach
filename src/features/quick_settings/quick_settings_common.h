#ifndef REACH_FEATURES_QUICK_SETTINGS_COMMON_H
#define REACH_FEATURES_QUICK_SETTINGS_COMMON_H

#include <stddef.h>
#include <stdint.h>

static inline float reach_quick_settings_clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

static inline float reach_quick_settings_clamp_min0(float value) {
  return value < 0.0f ? 0.0f : value;
}

static inline void reach_quick_settings_copy_utf16(uint16_t *dst,
                                                   size_t dst_count,
                                                   const uint16_t *src) {
  if (dst == nullptr || dst_count == 0) {
    return;
  }

  size_t index = 0;
  if (src != nullptr) {
    while (index + 1 < dst_count && src[index] != 0) {
      dst[index] = src[index];
      ++index;
    }
  }
  dst[index] = 0;
}

#endif
