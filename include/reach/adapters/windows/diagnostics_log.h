#ifndef REACH_ADAPTERS_WINDOWS_DIAGNOSTICS_LOG_H
#define REACH_ADAPTERS_WINDOWS_DIAGNOSTICS_LOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void reach_diagnostics_log_install(const char *process_name);
    void reach_diagnostics_log_write(const char *level, const char *message);
    int32_t reach_diagnostics_directory(wchar_t *out_path, size_t path_count);
    size_t reach_diagnostics_read_log(char *out, size_t capacity);
    size_t reach_diagnostics_read_notice(char *out, size_t capacity);
    void reach_diagnostics_clear_notice(void);
    int32_t reach_diagnostics_open_folder(void);

#ifdef __cplusplus
}
#endif

#endif
