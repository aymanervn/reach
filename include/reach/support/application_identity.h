#ifndef REACH_SUPPORT_APPLICATION_IDENTITY_H
#define REACH_SUPPORT_APPLICATION_IDENTITY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int32_t reach_application_identity_equal(const uint16_t *path_a, const uint16_t *aumid_a,
                                              const uint16_t *path_b,
                                              const uint16_t *aumid_b);

#ifdef __cplusplus
}
#endif

#endif
