#include "tt.h"
#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif
using namespace engine;
static inline uint64_t index_for_hash(uint64_t hash, uint64_t buckets) {
    if (buckets == 0)
        return 0;

#if defined(_MSC_VER)
    // MSVC: use _umul128 to get high 64 bits of 128-bit product
    unsigned long long high = 0;
    (void)_umul128((unsigned long long)hash, (unsigned long long)buckets, &high);
    return (uint64_t)high;
#elif defined(__SIZEOF_INT128__)
    // GCC/Clang: use __uint128_t
    __uint128_t prod = (__uint128_t)hash * (__uint128_t)buckets;
    return (uint64_t)(prod >> 64);
#else
    uint64_t aL = uint32_t(hash), aH = a >> 32;
    uint64_t bL = uint32_t(buckets), bH = b >> 32;
    uint64_t c1 = (aL * bL) >> 32;
    uint64_t c2 = aH * bL + c1;
    uint64_t c3 = aL * bH + uint32_t(c2);
    return aH * bH + (c2 >> 32) + (c3 >> 32);
#endif
}

void TranspositionTable::newSearch() { time++; }

void TranspositionTable::store(uint64_t hash, chess::Move best, int16_t score, int8_t depth, TTFlag flag) {
    // 2 entries per bucket
    if (buckets == 0)
        return;

    uint64_t index = index_for_hash(hash, buckets);

    TTEntry &e0 = table[2*index], &e1 = table[2*index + 1];
    // Store the entry
    for (TTEntry *e : { &e0, &e1 }) {
        if (e->key == hash || e->getDepth() < depth) {
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

TTEntry* TranspositionTable::lookup(uint64_t hash) {
    if (buckets == 0)
        return nullptr;

    uint64_t bucket = index_for_hash(hash, buckets);

    TTEntry& e0 = table[2 * bucket];
    TTEntry& e1 = table[2 * bucket + 1];

    if (e0.key == hash)
        return &e0;

    if (e1.key == hash)
        return &e1;

    return nullptr;
}