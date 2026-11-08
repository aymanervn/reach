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

#ifdef __cplusplus
}
#endif

#endif
