#include "windows_adapters_internal.h"

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

static const uint16_t *u16(const wchar_t *text)
{
    return reinterpret_cast<const uint16_t *>(text);
}

static int32_t matches(const wchar_t *shell, const wchar_t *path)
{
    return reach_windows_shell_value_is_reach(u16(shell), u16(path));
}

static void test_shell_value_matches_the_launch_command_or_the_bare_path(void)
{
    const wchar_t *path = L"C:\\Reach\\reach.exe";
    expect(matches(L"\"C:\\Reach\\reach.exe\" --launch", path), "quoted launch command matches");
    expect(matches(L"C:\\Reach\\reach.exe", path), "bare executable path matches");
    expect(matches(L"\"c:\\reach\\REACH.EXE\" --launch", path), "comparison ignores case");
}

static void test_shell_value_rejects_everything_else(void)
{
    const wchar_t *path = L"C:\\Reach\\reach.exe";
    expect(!matches(L"explorer.exe", path), "the Windows shell is not Reach");
    expect(!matches(L"\"C:\\Other\\reach.exe\" --launch", path), "another install is not this one");
    expect(!matches(L"\"C:\\Reach\\reach.exe\"", path),
           "a quoted path without the argument differs");
    expect(!matches(L"\"C:\\Reach\\reach.exe\" --launch --x", path), "extra arguments differ");
    expect(!matches(L"", path), "an empty shell is not Reach");
    expect(!matches(L"explorer.exe", L""), "an empty path never matches");
    expect(!reach_windows_shell_value_is_reach(nullptr, u16(path)), "a missing shell is refused");
    expect(!reach_windows_shell_value_is_reach(u16(path), nullptr), "a missing path is refused");
}

int main(void)
{
    test_shell_value_matches_the_launch_command_or_the_bare_path();
    test_shell_value_rejects_everything_else();
    return failed == 0 ? 0 : 1;
}
