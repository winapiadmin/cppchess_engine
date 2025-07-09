#pragma once
#include "chess.hpp"
#include "types.h"
#include <cstring>
#include "memory.h"  // Provides LargePagePtr and make_unique_large_page<T[]>

enum class TTFlag : uint8_t {
    EXACT,
    LOWERBOUND,
    UPPERBOUND
};

struct TTEntry {
    uint64_t         hash;
    uint16_t         move;
    Stockfish::Value score;
    TTFlag           flag;
    int8_t           depth;
    void             set(int8_t d, TTFlag f, Stockfish::Value s, chess::Move m) {
        depth = d;
        flag  = f;
        score = s;
        move  = m.move();
    }
};

class TranspositionTable {
    NNUEParser::LargePagePtr<TTEntry[]> table;  // Aligned (large-page) buffer
    int                                 size    = 0;
    int                                 buckets = 0;

   public:
    TranspositionTable() = default;

    TranspositionTable(int sizeInMB) { resize(sizeInMB); }

    void resize(int sizeInMB) {
        // Round down to power of 2
        while (sizeInMB & (sizeInMB - 1))
            sizeInMB >>= 1;

        int newSize = sizeInMB * 1048576 / sizeof(TTEntry);
        if (newSize == size)
        {
            clear();
            return;
        }

        table = NNUEParser::make_unique_large_page<TTEntry[]>(newSize);
        if (!table)
        {
            std::cerr << "info string TT allocation failed\n";
            std::exit(EXIT_FAILURE);
        }

        size    = newSize;
        buckets = size / 2;
        clear();
    }

    inline void clear() { std::fill_n(table.get(), size, TTEntry{}); }

    int      hashfull() const;    // Implement full-scan or sampling
    TTEntry* lookup(uint64_t h);  // Existing lookup logic
    void     store(uint64_t h, chess::Move m, Stockfish::Value s, int8_t d, TTFlag f);
};
