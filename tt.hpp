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
    uint64_t packed;            // Packed metadata field (now includes PV length)
    chess::Move pv[64];         // Vector to store the PV moves (variable length)

    uint8_t  depth()     const { return packed >> 56; }
    TTFlag   flag()      const { return static_cast<TTFlag>((packed >> 54) & 0x3); }
    int16_t  score()     const { return bestMove.score(); }
    uint64_t timestamp() const { return (packed >> 1) & 0x1FFFFFFFFFULL; }
    
    /** 
     * Checks if the transposition table entry is valid.
     * @return true if the entry is valid, false otherwise
     */
    bool     valid()     const { return packed & 1; }
    
    // 16-bit PV length getter
    uint16_t pvLength() const { return (packed >> 40) & 0xFFFF; }  // Extract 16-bit PV length
    
    // 16-bit PV length setter
    void setPVLength(uint16_t length) { 
        packed = (packed & ~(0xFFFFULL << 40)) | (uint64_t(length) << 40); // Set the 16-bit PV length in packed field
    }

    // Set function updated to handle PV length and PV moves
    void set(uint8_t d, TTFlag f, int16_t s, uint64_t ts, bool v, uint16_t pvLen, chess::Move* moves) {
        packed = (uint64_t(d) << 56) |
                 (uint64_t(f) << 54) |
                 ((ts & 0x1FFFFFFFFFULL) << 1) |
                 (v ? 1ULL : 0) |
                 (uint64_t(pvLen) << 40); // Set PV length in the packed field

        bestMove.setScore(s);
        memcpy(pv, moves, sizeof(chess::Move) * pvLen); // Copy PV moves
    }

    // Function to set a PV move at a specific index
    void setPVMove(uint16_t index, const chess::Move& move) {
        if (index < pvLength()) {
            pv[index] = move;
        }
    }

    // Function to get a PV move at a specific index
    chess::Move getPVMove(uint16_t index) const {
        if (index < pvLength()) {
            return pv[index];
        }
        return chess::Move();  // Return an invalid move if out of bounds
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
    void store(uint64_t hash, const chess::Move& bestMove, int16_t score, uint8_t depth, TTFlag flag, chess::Move* pv, uint16_t pvLen);
    TTEntry* probe(uint64_t hash);
};
