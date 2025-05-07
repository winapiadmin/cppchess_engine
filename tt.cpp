#include "tt.hpp"
const int8_t depthMargin=2;
TranspositionTable::TranspositionTable(size_t sizePow2)
    : sizeMask{(1ULL << sizePow2) - 1},
      entryCount(1ULL << sizePow2),
      currentTime(0)
{
    table = (TTEntry*)malloc(entryCount * 2 * sizeof(TTEntry)); // 2 entries per bucket
    clear();
}

TranspositionTable::~TranspositionTable() {
    free(this->table);
}
void TranspositionTable::clear() {
    std::memset(table, 0, sizeof(TTEntry) * entryCount*2);
}
void TranspositionTable::newSearch() {
    currentTime++;
}
void TranspositionTable::store(uint64_t hash, const chess::Move& bestMove, int16_t score, uint8_t depth, TTFlag flag) {
    size_t index = (hash & sizeMask) * 2; // Two entries per bucket
    TTEntry& e0 = table[index];
    TTEntry& e1 = table[index + 1];

    // Replace exact match or shallower
    for (TTEntry* e : {&e0, &e1}) {
        if (!e->valid() || e->hash == hash || e->depth()-depthMargin < depth) {
            e->hash = hash;
            e->bestMove = bestMove;
            e->set(depth, flag, score, currentTime, true);
            return;
        }
    }

    // No match: pick the one with oldest timestamp
    TTEntry* victim = (e0.timestamp() <= e1.timestamp()) ? &e0 : &e1;
    victim->hash = hash;
    victim->bestMove = bestMove;
    victim->set(depth, flag, score, currentTime, true);
}
TTEntry* TranspositionTable::probe(uint64_t hash) {
    size_t index = (hash & sizeMask) * 2;
    TTEntry& e0 = table[index];
    TTEntry& e1 = table[index + 1];

    if (e0.valid() && e0.hash == hash) {
        ++tthits;
        return &e0;
    }
    if (e1.valid() && e1.hash == hash) {
        ++tthits;
        return &e1;
    }

    ++ttmiss;
    return nullptr;
}
