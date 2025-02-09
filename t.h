#pragma once
//A weird header? but when remove it failes.
//THIS IS NOT TRANSPOSITION (it will be transposition.h)
#include "chess.hpp"
#include <cstdint>
#include <vector>
#ifdef USE_POPCNT
	#if defined(__GNUC__) || defined(__clang__)
		#define POPCOUNT64(x) __builtin_popcountll(x)
	#elif defined(_MSC_VER)
    	#include <intrin.h>
    	#define POPCOUNT64(x) __popcnt64(x)
	#elif defined(__INTEL_COMPILER)
    	#include <immintrin.h>
    	#define POPCOUNT64(x) _popcnt64(x)
	#else
    	#error "POPCOUNT64 is not supported on this compiler even if USE_POPCNT is defined"
	#endif
#else
    // Fallback manual implementation using Brian Kernighan's Algorithm
    inline int popcount(uint64_t n) {
        int count = 0;
        while (n) {
            n &= (n - 1); // Clear the least significant set bit
            count++;
        }
        return count;
    }
    #define POPCOUNT64(x) popcount(x)
#endif
#define countBits POPCOUNT64
#if defined(__GNUC__) || defined(__clang__)
#elif defined(_MSC_VER) // MSVC compiler
	// Cross-platform implementation of __builtin_ctzll
	inline int __builtin_ctzll(uint64_t x) {
    	if (x == 0) return 64; // Undefined behavior for 0, return max bits
    	
        unsigned long index;
        _BitScanForward64(&index, x);
        return static_cast<int>(index);
	
	}
#else
	static const uint8_t ctz_table[256] = {
    	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	
	uint64_t __builtin_ctzll(uint64_t x) {
    	uint8_t *bytes = (uint8_t *)&x;
    	int i;
    	for (i = 0; i < 8; i++) {
        	if (bytes[i] != 0) {
            	return i * 8 + ctz_table[bytes[i]];
        	}
    	}
    	return 64;
	}
	
#endif
using U64 = uint64_t;

// Constants for board files
constexpr U64 FILE_A = 0x0101010101010101ULL, RANK_1 = 0xff;
constexpr U64 FILE_B = FILE_A << 1,           RANK_2 = RANK_1 << 8;
constexpr U64 FILE_C = FILE_A << 2,           RANK_3 = RANK_2 << 8;
constexpr U64 FILE_D = FILE_A << 3,           RANK_4 = RANK_3 << 8;
constexpr U64 FILE_E = FILE_A << 4,           RANK_5 = RANK_4 << 8;
constexpr U64 FILE_F = FILE_A << 5,           RANK_6 = RANK_5 << 8;
constexpr U64 FILE_G = FILE_A << 6,           RANK_7 = RANK_6 << 8;
constexpr U64 FILE_H = FILE_A << 7,           RANK_8 = RANK_7 << 8;

constexpr U64 FILES[8] = {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H};
typedef unsigned long long Bitboard;
typedef char Square;
std::vector<Square> scan_reversed(Bitboard bb);

// Shift functions for pawn movement
inline U64 north(U64 b) { return b << 8; }
inline U64 south(U64 b) { return b >> 8; }
inline U64 east(U64 b) { return (b & ~FILE_H) << 1; }
inline U64 west(U64 b) { return (b & ~FILE_A) >> 1; }
inline U64 northEast(U64 b) { return (b & ~FILE_H) << 9; }
inline U64 northWest(U64 b) { return (b & ~FILE_A) << 7; }
inline U64 southEast(U64 b) { return (b & ~FILE_H) >> 7; }
inline U64 southWest(U64 b) { return (b & ~FILE_A) >> 9; }
#define getQueenAttacks(a,b) chess::attacks::queen(a,b).getBits()
#define getKnightAttacks(a,...) chess::attacks::knight(a).getBits()
#define getKingAttacks(a,friendlyPieces) chess::attacks::king(a).getBits()&~friendlyPieces
#define getRookAttacks(a,b) chess::attacks::rook(a,b).getBits()
#define getBishopAttacks(a,b) chess::attacks::bishop(a,b).getBits()
#define getQueenAttacks(a,b) chess::attacks::queen(a,b).getBits()
