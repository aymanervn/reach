#include "reach/support/crash_dump_prune.h"

void reach_crash_dump_prune_select(const reach_crash_dump_entry *entries, uint32_t count,
                                   uint32_t keep, uint64_t limit_bytes, int32_t *out_remove)
{
    if (entries == nullptr || out_remove == nullptr)
    {
        return;
    }

    uint32_t order[REACH_CRASH_DUMP_PRUNE_CAPACITY] = {};
    uint32_t sorted = 0;
    for (uint32_t index = 0; index < count; ++index)
    {
        out_remove[index] = 1;
        if (sorted == REACH_CRASH_DUMP_PRUNE_CAPACITY)
        {
            continue;
        }

        uint32_t slot = sorted++;
        while (slot > 0 && entries[order[slot - 1]].modified < entries[index].modified)
        {
            order[slot] = order[slot - 1];
            slot -= 1;
        }
        order[slot] = index;
    }

    uint32_t kept = 0;
    uint64_t total = 0;
    for (uint32_t rank = 0; rank < sorted && kept < keep; ++rank)
    {
        uint64_t size = entries[order[rank]].size;
        if (kept > 0 && (size > limit_bytes || total > limit_bytes - size))
        {
            break;
        }

        out_remove[order[rank]] = 0;
        total += size;
        kept += 1;
    }
}
