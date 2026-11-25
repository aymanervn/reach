#ifndef REACH_ADAPTERS_WINDOWS_CRASH_REPORT_H
#define REACH_ADAPTERS_WINDOWS_CRASH_REPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_CRASH_REPORT_BUILD_ID_CAPACITY 48u

    // Calling thread: any. The running executable's PDB signature GUID and age in hex, the key
    // debuggers and symbol stores match symbols by. Returns 0 and an empty string when the image
    // carries no CodeView record.
    int32_t reach_crash_report_build_id(char *out_id, size_t capacity);

    // Calling thread: the UI thread, once at startup after the dump prune. Logs how the previous
    // run ended and any dump written since it started, keeps the symbols of a build that left a
    // dump, drops symbols no dump needs, and records this run.
    void reach_crash_report_begin_run(void);

    // Calling thread: the UI thread, at a normal exit. Marks this run as ended cleanly.
    void reach_crash_report_end_run(void);

#ifdef __cplusplus
}
#endif

#endif
