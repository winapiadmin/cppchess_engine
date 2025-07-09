#include "tt.hpp"
void TranspositionTable::store(
  uint64_t hash, chess::Move best, Stockfish::Value score, int8_t depth, TTFlag flag) {
    // 2 entries per bucket
    if (buckets == 0)
        return;
    uint64_t index = (hash & (buckets - 1)) * 2;
    TTEntry &e0 = table[index], &e1 = table[index + 1];
    // Store the entry
    // Store the entry
    TTEntry* replace = &e0;  // Default to first entry
    for (TTEntry* e : {&e0, &e1})
    {
        if (e->hash == hash)
        {
            // Always replace exact hash match
            e->set(depth, flag, score, best);
            return;
        }
        if (e->depth < depth)
        {
            // Prefer entry with lower depth
            replace = e;
            break;
        }
        if (e->depth < replace->depth)
        {
            // Among candidates, choose the one with lowest depth
            replace = e;
        }
    }
    replace->hash = hash;
    replace->set(depth, flag, score, best);
}

TTEntry* TranspositionTable::lookup(uint64_t hash) {
    // 2 entries per bucket
    if (buckets == 0)
        return nullptr;
    uint64_t index = (hash & (buckets - 1)) * 2;
    TTEntry &e0 = table[index], &e1 = table[index + 1];
    // Check the entries
    for (TTEntry* e : {&e0, &e1})
    {
        if (e->hash == hash)
            return e;
    }
    return nullptr;
}
int TranspositionTable::hashfull() const {
    size_t count = 0;

    // Unroll loop for speed (process 4 entries per iteration)
    size_t i     = 0;
    size_t limit = size & ~3ULL;  // align to multiple of 4
    for (; i < limit; i += 4)
    {
        count += (table[i].hash != 0);
        count += (table[i + 1].hash != 0);
        count += (table[i + 2].hash != 0);
        count += (table[i + 3].hash != 0);
    }
    for (; i < size; ++i)
    {
        count += (table[i].hash != 0);
    }

    return static_cast<int>((count * 1000) / size);
}
