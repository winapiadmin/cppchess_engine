// File: tt.hpp
#pragma once

#include "chess.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>

inline uint64_t tthits = 0;
inline uint64_t ttmiss = 0;
inline uint64_t ttDepthHits[256]={};
enum TTFlag : uint8_t { EXACT = 0, LOWERBOUND = 1, UPPERBOUND = 2 };

struct TTEntry {
    uint64_t hash;              // 64-bit Zobrist hash
    chess::Move bestMove;       // Typically 16-bit move encoding
    uint64_t packed;            // Packed metadata field

    uint8_t  depth()     const { return packed >> 56; }
    TTFlag   flag()      const { return static_cast<TTFlag>((packed >> 54) & 0x3); }
    int16_t  score()     const { return bestMove.score(); }
    uint64_t timestamp() const { return (packed >> 1) & 0x1FFFFFFFFFULL; }
    bool     valid()     const { return packed & 1; }

    void set(uint8_t d, TTFlag f, int16_t s, uint64_t ts, bool v) {
        packed = (uint64_t(d) << 56) |
                 (uint64_t(f) << 54) |
                 ((ts & 0x1FFFFFFFFFULL) << 1) |
                 (v ? 1ULL : 0);
        bestMove.setScore(s);
    }
};

class TranspositionTable {
private:
    TTEntry* table = nullptr;
    size_t sizeMask = 0;
    size_t entryCount = 0;
    uint64_t currentTime = 0;

public:
    explicit TranspositionTable(size_t sizePow2);
    ~TranspositionTable();

    void clear();
    void clear_stats() {
        tthits = 0;
        ttmiss = 0;
        std::memset(ttDepthHits, 0, sizeof(ttDepthHits));
    }
    void newSearch();
    void store(uint64_t hash, const chess::Move& bestMove, int16_t score, uint8_t depth, TTFlag flag);
    TTEntry* probe(uint64_t hash);
};
