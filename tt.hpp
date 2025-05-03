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

struct TTEntry {
    uint64_t hash;              // 64-bit Zobrist hash
    chess::Move bestMove;       // Typically 16-bit move encoding
    uint64_t packed;            // Packed metadata field

    uint8_t  depth()     const { return packed >> 56; }
    uint8_t  flag()      const { return (packed >> 54) & 0x3; }
    int16_t  score()     const { return (int16_t)((packed >> 38) & 0xFFFF); }
    uint64_t timestamp() const { return (packed >> 1) & 0x1FFFFFFFFFULL; }
    bool     valid()     const { return packed & 1; }

    void set(uint8_t d, uint8_t f, int16_t s, uint64_t ts, bool v) {
        packed = (uint64_t(d) << 56) |
                 (uint64_t(f & 0x3) << 54) |
                 (uint64_t(uint16_t(s)) << 38) |
                 ((ts & 0x1FFFFFFFFFULL) << 1) |
                 (v ? 1ULL : 0);
    }
};


static_assert(sizeof(uint64_t) == 8, "Unexpected uint64_t size");
static_assert(sizeof(chess::Move) <= 4, "chess::Move should be compact");
static_assert(sizeof(uint64_t) * 2 + sizeof(chess::Move) <= 24, "TTEntry layout larger than expected");

#pragma message("[compile-time] ✅ TTEntry layout verified")
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
    void store(uint64_t hash, const chess::Move& bestMove, int16_t score, uint8_t depth, uint8_t flag);
    TTEntry* probe(uint64_t hash);
};