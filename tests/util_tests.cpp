#include "reach/support/util.h"

#include <stdio.h>

static int failed = 0;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failed += 1;
    }
}

int main(void)
{
    const uint16_t empty[] = {0};
    const uint16_t alpha[] = {'A', 'l', 'p', 'h', 'a', 0};
    const uint16_t alpha_copy[] = {'A', 'l', 'p', 'h', 'a', 0};
    const uint16_t alpha_lower[] = {'a', 'l', 'p', 'h', 'a', 0};
    const uint16_t alpha_short[] = {'A', 'l', 'p', 'h', 0};
    const uint16_t non_ascii[] = {0x00C9, 0};
    const uint16_t non_ascii_lower[] = {0x00E9, 0};

    expect(reach_strlen_utf16(nullptr) == 0, "null length");
    expect(reach_strlen_utf16(empty) == 0, "empty length");
    expect(reach_strlen_utf16(alpha) == 5, "text length");
    expect(reach_utf16_equal(nullptr, nullptr), "null equality");
    expect(!reach_utf16_equal(nullptr, empty), "one null inequality");
    expect(reach_utf16_equal(alpha, alpha_copy), "exact equality");
    expect(!reach_utf16_equal(alpha, alpha_lower), "exact case inequality");
    expect(!reach_utf16_equal(alpha, alpha_short), "exact length inequality");
    expect(reach_utf16_equal_ascii_case_insensitive(alpha, alpha_lower),
           "ASCII case-insensitive equality");
    expect(!reach_utf16_equal_ascii_case_insensitive(alpha, alpha_short),
           "ASCII case-insensitive length inequality");
    expect(!reach_utf16_equal_ascii_case_insensitive(non_ascii, non_ascii_lower),
           "non-ASCII remains exact");

    const uint16_t path_a[] = {'C', ':', '\\', 'A', 'p', 'p', '\\', 'X', '.', 'e', 'x', 'e', 0};
    const uint16_t path_b[] = {'c', ':', '/', 'a', 'P', 'P', '/', 'x', '.', 'E', 'X', 'E', 0};
    const uint16_t path_short[] = {'C', ':', '\\', 'A', 'p', 'p', 0};
    expect(reach_path_equals(path_a, path_b), "path case and separator equality");
    expect(!reach_path_equals(path_a, path_short), "path length inequality");
    expect(reach_path_equals(nullptr, nullptr), "null path equality");
    expect(!reach_path_equals(nullptr, path_a), "one null path inequality");

    uint16_t copy[6] = {};
    expect(reach_copy_utf16(copy, 6, alpha) == REACH_OK && reach_utf16_equal(copy, alpha),
           "UTF-16 exact copy");
    expect(reach_copy_utf16(copy, 5, alpha) == REACH_ERROR && copy[4] == 0,
           "UTF-16 truncated copy");
    expect(reach_copy_utf16(copy, 1, alpha) == REACH_ERROR && copy[0] == 0, "UTF-16 one-unit copy");
    expect(reach_copy_utf16(nullptr, 6, alpha) == REACH_INVALID_ARGUMENT,
           "UTF-16 invalid destination");

    expect(reach_copy_ascii_to_utf16(copy, 6, "Alpha") == REACH_OK &&
               reach_utf16_equal(copy, alpha),
           "ASCII exact copy");
    expect(reach_copy_ascii_to_utf16(copy, 5, "Alpha") == REACH_ERROR && copy[4] == 0,
           "ASCII truncated copy");
    expect(reach_copy_ascii_to_utf16(copy, 1, "Alpha") == REACH_ERROR && copy[0] == 0,
           "ASCII one-unit copy");
    expect(reach_copy_ascii_to_utf16(copy, 6, nullptr) == REACH_INVALID_ARGUMENT,
           "ASCII invalid source");

    return failed == 0 ? 0 : 1;
}
