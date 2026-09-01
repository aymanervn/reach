#include <windows.h>
#include <shellapi.h>

extern "C" int reach_windows_update_helper_run(const wchar_t *request_path,
                                               const wchar_t *response_path,
                                               const wchar_t *progress_section_name);

int WINAPI wWinMain(HINSTANCE, HINSTANCE, wchar_t *, int)
{
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr || argc < 3 || argc > 4)
    {
        if (argv != nullptr)
            LocalFree(argv);
        return 2;
    }
    int result = reach_windows_update_helper_run(argv[1], argv[2], argc == 4 ? argv[3] : nullptr);
    LocalFree(argv);
    return result;
}
