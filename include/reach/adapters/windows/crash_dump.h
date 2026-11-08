#ifndef REACH_ADAPTERS_WINDOWS_CRASH_DUMP_H
#define REACH_ADAPTERS_WINDOWS_CRASH_DUMP_H

#ifdef __cplusplus
extern "C"
{
#endif

    void reach_crash_dump_install(const char *process_name);
    void reach_crash_dump_prune(void);

#ifdef __cplusplus
}
#endif

#endif
