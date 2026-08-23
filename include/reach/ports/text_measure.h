#ifndef REACH_PORTS_TEXT_MEASURE_H
#define REACH_PORTS_TEXT_MEASURE_H

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

#ifdef __cplusplus
}
#endif

#endif
