#ifndef REACH_DIAGNOSTICS_LOG_H
#define REACH_DIAGNOSTICS_LOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_DIAGNOSTICS_LOG_LIMIT_BYTES (512u * 1024u)
#define REACH_DIAGNOSTICS_LOG_KEEP 2u
#define REACH_DIAGNOSTICS_LINE_CAPACITY 1024u

    typedef struct reach_log_timestamp
    {
        uint16_t year;
        uint16_t month;
        uint16_t day;
        uint16_t hour;
        uint16_t minute;
        uint16_t second;
        uint16_t millisecond;
    } reach_log_timestamp;

    typedef struct reach_log_rotation_step
    {
        int32_t from;
        int32_t to;
    } reach_log_rotation_step;

    int32_t reach_log_rotation_needed(uint64_t current_size, uint64_t incoming_size,
                                      uint64_t limit);
    uint32_t reach_log_rotation_steps(uint32_t keep, reach_log_rotation_step *out_steps,
                                      uint32_t capacity);
    size_t reach_log_format_line(char *out, size_t capacity, const reach_log_timestamp *time,
                                 const char *process, uint32_t process_id, const char *level,
                                 const char *message);

#ifdef __cplusplus
}
#endif

#endif
