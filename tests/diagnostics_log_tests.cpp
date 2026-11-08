#include "reach/support/diagnostics_log.h"

#include <stdio.h>
#include <string.h>

static int failed = 0;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failed += 1;
    }
}

static void test_rotation_triggers_only_when_the_limit_would_be_exceeded(void)
{
    expect(!reach_log_rotation_needed(0, 4096, 1024), "an empty log never rotates");
    expect(!reach_log_rotation_needed(1000, 24, 1024), "a line that exactly fits stays");
    expect(reach_log_rotation_needed(1000, 25, 1024), "a line past the limit rotates");
}

static void test_rotation_steps_delete_oldest_then_shift(void)
{
    reach_log_rotation_step steps[8] = {};
    uint32_t count = reach_log_rotation_steps(2, steps, 8);
    expect(count == 3, "keeping two archives takes three steps");
    expect(steps[0].from == 2 && steps[0].to == -1, "the oldest archive is deleted first");
    expect(steps[1].from == 1 && steps[1].to == 2, "the newer archive becomes the oldest");
    expect(steps[2].from == 0 && steps[2].to == 1, "the live log becomes the newest archive");
    expect(reach_log_rotation_steps(2, steps, 2) == 0, "a short buffer yields no steps");
    expect(reach_log_rotation_steps(0, steps, 8) == 0, "keeping nothing yields no steps");
}

static void test_line_format_is_one_sanitized_line(void)
{
    reach_log_timestamp stamp = {2026, 11, 8, 12, 34, 56, 789};
    char line[256] = {};
    size_t length = reach_log_format_line(line, sizeof(line), &stamp, "reach", 1234, "error",
                                          "device lost\r\nop=EndDraw");
    expect(strcmp(line,
                  "2026-11-08T12:34:56.789Z reach pid=1234 error device lost  op=EndDraw\n") == 0,
           "line carries timestamp, process, pid, level and a single-line message");
    expect(length == strlen(line), "returned length matches the line");

    char small[40] = {};
    length = reach_log_format_line(small, sizeof(small), &stamp, "reach", 1, "info", "x");
    expect(length == strlen(small) && small[length - 1] == '\n',
           "an oversized header is truncated and still ends the line");

    char tight[80] = {};
    length = reach_log_format_line(tight, sizeof(tight), &stamp, "reach", 1, "info",
                                   "0123456789012345678901234567890123456789");
    expect(length < sizeof(tight) && tight[length - 1] == '\n' && tight[length] == 0,
           "a long message is truncated inside the buffer");
}

int main(void)
{
    test_rotation_triggers_only_when_the_limit_would_be_exceeded();
    test_rotation_steps_delete_oldest_then_shift();
    test_line_format_is_one_sanitized_line();
    return failed == 0 ? 0 : 1;
}
