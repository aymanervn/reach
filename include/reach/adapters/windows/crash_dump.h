#ifndef REACH_ADAPTERS_WINDOWS_CRASH_DUMP_H
#define REACH_ADAPTERS_WINDOWS_CRASH_DUMP_H

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void reach_crash_dump_install(const char *process_name);
    void reach_crash_dump_prune(void);

    // Points Windows Error Reporting's dumps for Reach's executables at the diagnostics dump
    // folder. Needs an elevated caller.
    reach_result reach_crash_dump_register_local_dumps(void);

#ifdef __cplusplus
}
#endif

#endif
