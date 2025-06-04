#include "tt.hpp"

void TranspositionTable::newSearch()
{
    time = 0;
}

void TranspositionTable::store(uint64_t hash, chess::Move best, int16_t score, int8_t depth, TTFlag flag)
{
    // 2 entries per bucket
    if (buckets == 0)
        return;
    uint64_t index = hash % buckets;
    if (index >= buckets - 1)
        index = buckets - 2; // Ensure we don't overflow
    TTEntry &e0 = table[index], &e1 = table[index + 1];
    // Store the entry

    ++time; // monotonically increasing stamp
    for (TTEntry *e : {&e0, &e1})
    {
        if (!e->valid() || e->hash == hash || e->depth() < depth)
        {
            e->hash = hash;
            e->set(depth, flag, score, time, true, best);
            return;
        }
    }
    // If we get here, we need to evict an entry
    // Find the oldest entry
    TTEntry *oldest = (e0.timestamp() < e1.timestamp()) ? &e0 : &e1;
    // Evict it
    oldest->hash = hash;
    oldest->set(depth, flag, score, time, true, best);
}

TTEntry *TranspositionTable::lookup(uint64_t hash)
{
    // 2 entries per bucket
    if (buckets == 0)
        return nullptr;
    uint64_t index = hash % buckets;
    if (index >= buckets - 1)
        index = buckets - 2; // Ensure we don't overflow
    TTEntry &e0 = table[index], &e1 = table[index + 1];
    // Check the entries
    for (TTEntry *e : {&e0, &e1})
    {
        if (e->valid() && e->hash == hash)
            return e;
    }
    return nullptr;
}
