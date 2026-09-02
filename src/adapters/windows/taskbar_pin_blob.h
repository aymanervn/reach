#ifndef REACH_TASKBAR_PIN_BLOB_H
#define REACH_TASKBAR_PIN_BLOB_H

#include "reach/support/util.h"

#include <stddef.h>
#include <stdint.h>

typedef int32_t (*reach_taskbar_pin_blob_visitor)(const uint8_t *pidl, size_t size, void *user);

reach_result reach_taskbar_pin_blob_visit(const uint8_t *data, size_t size,
                                          reach_taskbar_pin_blob_visitor visitor, void *user);

#endif
