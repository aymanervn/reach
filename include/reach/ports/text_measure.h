#ifndef REACH_PORTS_TEXT_MEASURE_H
#define REACH_PORTS_TEXT_MEASURE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef reach_result (*reach_text_measure_fn)(void *context, const uint16_t *text,
                                                  float text_size, int32_t text_weight,
                                                  float *out_width);

    typedef struct reach_text_measure_port
    {
        void *context;
        reach_text_measure_fn measure;
    } reach_text_measure_port;

    static inline float reach_text_width_or_estimate(const reach_text_measure_port *port,
                                                     const uint16_t *text, float text_size,
                                                     int32_t text_weight,
                                                     float fallback_glyph_advance_ratio)
    {
        if (text == NULL || text_size <= 0.0f)
        {
            return 0.0f;
        }

        float width = 0.0f;
        if (port != NULL && port->measure != NULL &&
            port->measure(port->context, text, text_size, text_weight, &width) == REACH_OK &&
            width >= 0.0f)
        {
            return width;
        }

        size_t length = 0;
        while (text[length] != 0)
        {
            ++length;
        }
        float ratio = fallback_glyph_advance_ratio > 0.0f ? fallback_glyph_advance_ratio : 0.0f;
        return (float)length * text_size * ratio;
    }

#ifdef __cplusplus
}
#endif

#endif
