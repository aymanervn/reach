#ifndef REACH_CRASH_DUMP_PRUNE_H
#define REACH_CRASH_DUMP_PRUNE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_CRASH_DUMP_KEEP 5u
#define REACH_CRASH_DUMP_LIMIT_BYTES (50ull * 1024ull * 1024ull)
#define REACH_CRASH_DUMP_PRUNE_CAPACITY 256u

    typedef struct reach_crash_dump_entry
    {
        uint64_t size;
        uint64_t modified;
    } reach_crash_dump_entry;

    void reach_crash_dump_prune_select(const reach_crash_dump_entry *entries, uint32_t count,
                                       uint32_t keep, uint64_t limit_bytes, int32_t *out_remove);

#ifdef __cplusplus
}
#endif

#endif
