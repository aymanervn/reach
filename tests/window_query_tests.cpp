#include "../src/adapters/windows/window_management/window_query_win32.h"

#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static const uint16_t *wide(const wchar_t *value)
{
    return reinterpret_cast<const uint16_t *>(value);
}

int main(void)
{
    expect_true(reach_window_identity_is_explorer_dialog(
                    wide(L"C:\\Windows\\explorer.exe"), wide(L"OperationStatusWindow")),
                "copy operation window is an Explorer dialog");
    expect_true(reach_window_identity_is_explorer_dialog(
                    wide(L"c:\\windows\\EXPLORER.EXE"), wide(L"#32770")),
                "standard Explorer dialog matching is case insensitive");
    expect_true(!reach_window_identity_is_explorer_dialog(
                    wide(L"C:\\Windows\\explorer.exe"), wide(L"CabinetWClass")),
                "Explorer folder windows are not dialogs");
    expect_true(!reach_window_identity_is_explorer_dialog(
                    wide(L"C:\\Tools\\explorer.exe.backup"), wide(L"OperationStatusWindow")),
                "similarly named executables are rejected");
    expect_true(!reach_window_identity_is_explorer_dialog(
                    wide(L"C:\\Tools\\other.exe"), wide(L"#32770")),
                "dialogs from other applications are rejected");

    if (failures == 0)
    {
        printf("window query tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
