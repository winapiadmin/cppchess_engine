#include "tt.h"
#include <cstdint>

#if defined(_MSC_VER)
#  include <intrin.h>
#endif
using namespace engine;
static inline uint64_t index_for_hash(uint64_t hash, uint64_t buckets)
{
    if (buckets == 0)
        return 0;

#if defined(_MSC_VER)
    // MSVC: use _umul128 to get high 64 bits of 128-bit product
    unsigned long long high = 0;
    (void)_umul128((unsigned long long)hash, (unsigned long long)buckets, &high);
    return (uint64_t)high;
#elif defined(__SIZEOF_INT128__)
    // GCC/Clang: use __uint128_t
    __uint128_t prod = ( __uint128_t)hash * ( __uint128_t)buckets;
    return (uint64_t)(prod >> 64);
#else
    // Portable fallback (rare): fall back to modulo if no 128-bit or _umul128 available.
    // This is only used on very uncommon toolchains; primary implementations above avoid division.
    return hash % buckets;
#endif
}

void TranspositionTable::newSearch()
{
    time++;
}

void TranspositionTable::store(uint64_t hash, chess::Move best, int16_t score, int8_t depth, TTFlag flag)
{
    // 2 entries per bucket
    if (buckets == 0)
        return;

    uint64_t index = index_for_hash(hash, buckets);
    if (index >= buckets - 1)
        index = buckets - 2; // Ensure we don't overflow

    TTEntry &e0 = table[index], &e1 = table[index + 1];
    // Store the entry
    for (TTEntry *e : { &e0, &e1 })
    {
        if (e->key == hash || e->getDepth() < depth)
        {
            e->key = hash;
            e->setPackedFields(score, depth, flag, best.raw(), time);

            return;
        }
    }
    // If we get here, we need to evict an entry
    // Find the oldest entry
    TTEntry *oldest = (e0.timestamp() < e1.timestamp()) ? &e0 : &e1;
    // Evict it
    oldest->key = hash;
    oldest->setPackedFields(score, depth, flag, best.raw(), time);
}

TTEntry *TranspositionTable::lookup(uint64_t hash)
{
    // 2 entries per bucket
    if (buckets == 0)
        return nullptr;

    uint64_t index = index_for_hash(hash, buckets);
    if (index >= buckets - 1)
        index = buckets - 2; // Ensure we don't overflow

    TTEntry &e0 = table[index], &e1 = table[index + 1];
    // Check the entries
    for (TTEntry *e : { &e0, &e1 })
    {
        if (e->key == hash)
            return e;
    }
    return nullptr;
}