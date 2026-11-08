#include "reach/support/diagnostics_log.h"

#include <stdio.h>
#include <string.h>

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

static int reach_log_parse_digits(const char *text, size_t length, size_t offset, size_t count,
                                  uint16_t *out)
{
    if (offset + count > length)
    {
        return 0;
    }

    uint32_t value = 0;
    for (size_t index = 0; index < count; ++index)
    {
        char digit = text[offset + index];
        if (digit < '0' || digit > '9')
        {
            return 0;
        }
        value = value * 10 + (uint32_t)(digit - '0');
    }
    *out = (uint16_t)value;
    return 1;
}

int32_t reach_log_parse_timestamp(const char *text, size_t length, reach_log_timestamp *out)
{
    if (text == nullptr || out == nullptr || length < 19 || text[4] != '-' || text[7] != '-' ||
        text[10] != 'T' || text[13] != ':' || text[16] != ':')
    {
        return 0;
    }

    reach_log_timestamp parsed = {};
    if (!reach_log_parse_digits(text, length, 0, 4, &parsed.year) ||
        !reach_log_parse_digits(text, length, 5, 2, &parsed.month) ||
        !reach_log_parse_digits(text, length, 8, 2, &parsed.day) ||
        !reach_log_parse_digits(text, length, 11, 2, &parsed.hour) ||
        !reach_log_parse_digits(text, length, 14, 2, &parsed.minute) ||
        !reach_log_parse_digits(text, length, 17, 2, &parsed.second))
    {
        return 0;
    }

    if (length >= 24 && text[19] == '.' &&
        !reach_log_parse_digits(text, length, 20, 3, &parsed.millisecond))
    {
        return 0;
    }

    *out = parsed;
    return 1;
}

static int reach_diagnostics_line_has_event(const char *line, size_t length, const char *event)
{
    size_t event_length = strlen(event);
    for (size_t index = 0; index + 1 + event_length <= length; ++index)
    {
        if (line[index] == ' ' && memcmp(line + index + 1, event, event_length) == 0)
        {
            return 1;
        }
    }
    return 0;
}

void reach_diagnostics_summarize(const char *text, size_t length, reach_diagnostics_summary *out)
{
    if (out == nullptr)
    {
        return;
    }

    *out = {};
    size_t start = 0;
    while (text != nullptr && start < length)
    {
        size_t end = start;
        while (end < length && text[end] != '\n')
        {
            ++end;
        }

        if (end > start)
        {
            const char *line = text + start;
            size_t line_length = end - start;
            reach_log_timestamp stamp = {};
            if (reach_log_parse_timestamp(line, line_length, &stamp))
            {
                out->event_count += 1;
                if (!out->has_first)
                {
                    out->has_first = 1;
                    out->first = stamp;
                }
                if (reach_diagnostics_line_has_event(line, line_length, "device_recovered"))
                {
                    out->has_recovery = 1;
                    out->last_recovery = stamp;
                }
            }
        }
        start = end + 1;
    }
}

size_t reach_diagnostics_format_summary(char *out, size_t capacity,
                                        const reach_diagnostics_summary *summary)
{
    if (out == nullptr || capacity == 0)
    {
        return 0;
    }

    out[0] = 0;
    if (summary == nullptr || summary->event_count == 0 || !summary->has_first)
    {
        int written = snprintf(out, capacity, "No events recorded");
        return written > 0 ? (size_t)written : 0;
    }

    int written = snprintf(out, capacity, "%u events since %04u-%02u-%02u",
                           (unsigned)summary->event_count, (unsigned)summary->first.year,
                           (unsigned)summary->first.month, (unsigned)summary->first.day);
    if (written < 0 || (size_t)written >= capacity)
    {
        return 0;
    }

    if (summary->has_recovery)
    {
        int extra =
            snprintf(out + written, capacity - (size_t)written,
                     ", last device recovery %04u-%02u-%02u %02u:%02u",
                     (unsigned)summary->last_recovery.year, (unsigned)summary->last_recovery.month,
                     (unsigned)summary->last_recovery.day, (unsigned)summary->last_recovery.hour,
                     (unsigned)summary->last_recovery.minute);
        if (extra > 0 && (size_t)(written + extra) < capacity)
        {
            written += extra;
        }
        else
        {
            out[written] = 0;
        }
    }
    return (size_t)written;
}

size_t reach_diagnostics_format_notice(char *out, size_t capacity, const reach_log_timestamp *stamp)
{
    if (out == nullptr || capacity == 0)
    {
        return 0;
    }

    int written =
        stamp != nullptr
            ? snprintf(out, capacity,
                       "Reach fell back to Windows Explorer on %04u-%02u-%02u; crash log saved",
                       (unsigned)stamp->year, (unsigned)stamp->month, (unsigned)stamp->day)
            : snprintf(out, capacity, "Reach fell back to Windows Explorer; crash log saved");
    if (written < 0 || (size_t)written >= capacity)
    {
        out[0] = 0;
        return 0;
    }
    return (size_t)written;
}
