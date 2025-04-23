#pragma once
#include "chess.hpp"
#include <map>
#include <vector>
#include <cstdint>
#if __cplusplus >= 202002L
    #define POPCOUNT64(bits) std::popcount(bits);
#elif defined(USE_POPCNT)
	#if defined(__GNUC__) || defined(__clang__)
		#define POPCOUNT64(x) __builtin_popcountll(x)
	#elif defined(_MSC_VER)
    	#include <intrin.h>
    	#define POPCOUNT64(x) __popcnt64(x)
	#elif defined(__INTEL_COMPILER)
    	#include <immintrin.h>
    	#define POPCOUNT64(x) _popcnt64(x)
	#else
    	#error "Use C++20, it provides popcount"
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

#if __cplusplus >= 202002L
    #define lsb(bits) std::countl_zero(bits) ^ 63;
#elif defined(__GNUC__) || defined(__clang__)
    #define lsb __builtin_ctzll
#elif defined(_MSC_VER) // MSVC compiler
	// Cross-platform implementation of __builtin_ctzll
	inline int lsb(uint64_t x) {
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
	
	uint64_t lsb(uint64_t x) {
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
namespace chess{
	class Position : public Board {
   	public:
    	Position() : Board() {}
    	Position(std::string_view fen) : Board(fen) {}
	
    	void makeMove(const Move& move) {
        	move_stack.push_back(move);
        	Board::makeMove(move);
    	}
	
    	Move pop() {
        	if (move_stack.empty()) {
            	throw std::runtime_error("No moves to undo");
        	}
	
        	const auto& move = move_stack.back();
        	Board::unmakeMove(move);
        	move_stack.pop_back();
        	return move;
    	}
    	std::vector<Move> move_stack;
	};
}

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
#define getKingAttacks(a,friendlyPieces) (chess::attacks::king(a).getBits()&~(friendlyPieces))
#define getRookAttacks(a,b) chess::attacks::rook(a,b).getBits()
#define getBishopAttacks(a,b) chess::attacks::bishop(a,b).getBits()
#define getPawnAttacks(a,b) chess::attacks::pawn(a,b).getBits()
#define MAX_PLY 245
#define MAX 32767 // for black
#define MAX_MATE 32000
#define MATE(i) (MAX-i)
#define MATE_DISTANCE(i) (i-MAX_MATE)
int eval(chess::Position);
int piece_value(chess::PieceType piece);
