#include "reach/support/diagnostics_log.h"

#include <stdio.h>

int32_t reach_log_rotation_needed(uint64_t current_size, uint64_t incoming_size, uint64_t limit)
{
    return current_size > 0 && current_size + incoming_size > limit;
}

uint32_t reach_log_rotation_steps(uint32_t keep, reach_log_rotation_step *out_steps,
                                  uint32_t capacity)
{
    if (out_steps == nullptr || keep == 0 || capacity < keep + 1)
    {
        return 0;
    }

    uint32_t count = 0;
    out_steps[count++] = {(int32_t)keep, -1};
    for (uint32_t generation = keep; generation > 0; --generation)
    {
        out_steps[count++] = {(int32_t)(generation - 1), (int32_t)generation};
    }
    return count;
}

size_t reach_log_format_line(char *out, size_t capacity, const reach_log_timestamp *time,
                             const char *process, uint32_t process_id, const char *level,
                             const char *message)
{
    if (out == nullptr || capacity < 2 || time == nullptr)
    {
        return 0;
    }

    int header = snprintf(out, capacity, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ %s pid=%u %s ",
                          (unsigned)time->year, (unsigned)time->month, (unsigned)time->day,
                          (unsigned)time->hour, (unsigned)time->minute, (unsigned)time->second,
                          (unsigned)time->millisecond, process != nullptr ? process : "unknown",
                          (unsigned)process_id, level != nullptr ? level : "info");
    if (header < 0 || (size_t)header >= capacity - 1)
    {
        out[capacity - 2] = '\n';
        out[capacity - 1] = 0;
        return capacity - 1;
    }

    size_t length = (size_t)header;
    for (const char *cursor = message; cursor != nullptr && *cursor != 0 && length < capacity - 2;
         ++cursor)
    {
        char value = *cursor;
        out[length++] = value == '\r' || value == '\n' ? ' ' : value;
    }
    out[length++] = '\n';
    out[length] = 0;
    return length;
}
