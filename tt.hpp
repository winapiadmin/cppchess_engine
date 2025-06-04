#pragma once
#include "chess.hpp"
#include <cmath>
#include <cstring>
enum class TTFlag : uint8_t
{
    EXACT = 0,
    LOWERBOUND = 1,
    UPPERBOUND = 2,
    // 3 is unused or reserved
};

struct TTEntry
{
    uint64_t hash;
    chess::Move bestMove;
    uint64_t packed;

    // Getters
    uint8_t depth() const
    {
        return ((packed >> 58) & 0x3F) + 1; // 6 bits depth-1
    }

    TTFlag flag() const
    {
        return static_cast<TTFlag>((packed >> 56) & 0x3);
    }

    uint64_t timestamp() const
    {
        return (packed >> 17) & 0x7FFFFFFFFFULL; // 39 bits
    }

    bool valid() const
    {
        return (packed & 1) != 0;
    }

    int16_t score() const
    {
        return bestMove.score();
    }

    // Setter
    void set(uint8_t d, TTFlag f, int16_t s, uint64_t ts, bool v, chess::Move move)
    {
        // Store depth - 1 in 6 bits, clamp at 63
        uint8_t depth_encoded = (d > 0) ? (d - 1) : 0;
        if (depth_encoded > 63)
            depth_encoded = 63;

        packed = (uint64_t(depth_encoded) << 58) |
                 (uint64_t((int)f & 0x3) << 56) |
                 ((ts & 0x7FFFFFFFFFULL) << 17) |
                 (v);
        bestMove=move;
        bestMove.setScore(s);
    }
};
class TranspositionTable
{
    TTEntry *table;
    int buckets; // number of buckets (pairs)
    uint32_t time;

public:
    int size; // total number of TTEntry elements (must be even)
    TranspositionTable() : table(nullptr), buckets(0), time(0), size(0) {}

    TranspositionTable(int sizeInMB) : time(0)
    {
        size = sizeInMB * 1048576 / sizeof(TTEntry);
        if (size % 2 != 0)
            size--; // Ensure even size
        buckets = size / 2;
        table = new TTEntry[size];
        clear();
    }

    ~TranspositionTable()
    {
        delete[] table;
    }

    void resize(int sizeInMB)
    {
        TTEntry *old_table = table;
        int old_size = size;

        size = sizeInMB * 1048576 / sizeof(TTEntry);
        if (size % 2 != 0)
            size--;
        buckets = size / 2;

        TTEntry *new_table = new (std::nothrow) TTEntry[size];
        if (!new_table)
        {
            // Restore old values on failure
            table = old_table;
            size = old_size;
            buckets = old_size / 2;
            throw std::bad_alloc();
        }

        std::memcpy(new_table, old_table,
                    sizeof(TTEntry) * std::min(size, old_size));
        delete[] old_table;
        table  = new_table;
        buckets = size / 2;
    }

    void newSearch();
    void store(uint64_t hash, chess::Move best, int16_t score, int8_t depth, TTFlag flag);

    inline void clear()
    {
        std::fill_n(table, size, TTEntry{});
    }

    TTEntry *lookup(uint64_t hash);
};
