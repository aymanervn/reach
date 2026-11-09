#include "package_store_path.h"

#include <wchar.h>

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main()
{
    int failed = 0;
    wchar_t family[64] = {};
    const wchar_t *relative = nullptr;

    failed += expect(reach_package_store_path_split(
        L"C:\\Program Files\\WindowsApps\\Claude_3.6556.2.0_x64__pzs8sxrjxfjjc\\app\\claude.exe",
        family, 64, &relative));
    failed += expect(wcscmp(family, L"Claude_pzs8sxrjxfjjc") == 0);
    failed += expect(relative != nullptr && wcscmp(relative, L"app\\claude.exe") == 0);

    failed += expect(reach_package_store_path_split(
        L"D:\\windowsapps\\OpenAI.Codex_26.1013.4894.0_x64_~_2p2nqsd0c76g0\\Codex.exe", family, 64,
        &relative));
    failed += expect(wcscmp(family, L"OpenAI.Codex_2p2nqsd0c76g0") == 0);
    failed += expect(relative != nullptr && wcscmp(relative, L"Codex.exe") == 0);

    failed += expect(
        !reach_package_store_path_split(L"C:\\Program Files\\Zed\\Zed.exe", family, 64, &relative));
    failed += expect(!reach_package_store_path_split(
        L"C:\\Program Files\\WindowsApps\\Claude_3.6556.2.0_x64__pzs8sxrjxfjjc", family, 64,
        &relative));
    failed += expect(!reach_package_store_path_split(
        L"C:\\Program Files\\WindowsApps\\NotAPackage\\app.exe", family, 64, &relative));
    failed += expect(!reach_package_store_path_split(
        L"C:\\Program Files\\WindowsApps\\Claude_3.6556.2.0_x64__pzs8sxrjxfjjc\\app\\claude.exe",
        family, 8, &relative));
    return failed == 0 ? 0 : 1;
}
