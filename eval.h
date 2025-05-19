#pragma once
#include "chess.hpp"
#include <map>
#include <vector>
#include <cstdint>
#include <cstring>
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
inline int popcount(uint64_t n)
{
	int count = 0;
	while (n)
	{
		n &= (n - 1); // Clear the least significant set bit
		count++;
	}
	return count;
}
#define POPCOUNT64(x) popcount(x)
#endif
#define countBits POPCOUNT64

#if __cplusplus >= 202002L
inline int lsb(uint64_t &p) { return std::countl_zero(p) ^ 63; }
#elif defined(__GNUC__) || defined(__clang__)
inline int lsb(uint64_t p) { return __builtin_ctzll(p); }
#elif defined(_MSC_VER) // MSVC compiler
// Cross-platform implementation of __builtin_ctzll
inline int lsb(uint64_t x)
{
	if (x == 0)
		return 64; // Undefined behavior for 0, return max bits

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
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

uint64_t lsb(uint64_t x)
{
	uint8_t *bytes = (uint8_t *)&x;
	int i;
	for (i = 0; i < 8; i++)
	{
		if (bytes[i] != 0)
		{
			return i * 8 + ctz_table[bytes[i]];
		}
	}
	return 64;
}
#endif
#if defined(__GNUC__) || defined(__clang__)
#define RESTRICT __restrict__
#elif defined(_MSC_VER)
#define RESTRICT __restrict
#else
#define RESTRICT
#endif
class MoveStack {
	public:
	 using value_type      = chess::Move;
	 using size_type       = int;
	 using difference_type = std::ptrdiff_t;
	 using reference       = value_type&;
	 using const_reference = const value_type&;
	 using pointer         = value_type*;
	 using const_pointer   = const value_type*;
 
	 using iterator       = value_type*;
	 using const_iterator = const value_type*;
 
	 using reverse_iterator       = std::reverse_iterator<iterator>;
	 using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	 [[nodiscard]] constexpr reference front() noexcept { return moves_[0]; }
	 [[nodiscard]] constexpr const_reference front() const noexcept { return moves_[0]; }
 
	 [[nodiscard]] constexpr reference back() noexcept { return moves_[size_ - 1]; }
	 [[nodiscard]] constexpr const_reference back() const noexcept { return moves_[size_ - 1]; }
 
	 // Iterators
 
	 [[nodiscard]] constexpr iterator begin() noexcept { return &moves_[0]; }
	 [[nodiscard]] constexpr const_iterator begin() const noexcept { return &moves_[0]; }
 
	 [[nodiscard]] constexpr iterator end() noexcept { return &moves_[0] + size_; }
	 [[nodiscard]] constexpr const_iterator end() const noexcept { return &moves_[0] + size_; }
 
	 // Capacity
 
	 /**
	  * @brief Checks if the movelist is empty.
	  * @return
	  */
	 [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
 
	 /**
	  * @brief Return the number of moves in the movelist.
	  * @return
	  */
	 [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
 
	 // Modifiers
 
	 /**
	  * @brief Clears the movelist.
	  */
	 constexpr void clear() noexcept { size_ = 0; }
 
	 /**
	  * @brief Add a move to the end of the movelist.
	  * @param move
	  */
	 constexpr void push(const_reference move) noexcept {
		 assert(size_ < 256);
		 moves_[size_++] = move;
	 }
 
	 /**
	  * @brief Add a move to the end of the movelist.
	  * @param move
	  */
	 constexpr void push(value_type&& move) noexcept {
		 assert(size_ < 256);
		 moves_[size_++] = std::move(move);
	 }
 
	 constexpr auto pop() noexcept -> value_type {
		 assert(size_ > 0);
		 return moves_[--size_];
	 }

	private:
	 std::array<value_type, 256> moves_;
	 size_type size_ = 0;
 };

namespace chess
{
	class Position : public Board
	{
	public:
		Position() : Board(), move_stack() {}
		Position(std::string_view fen) : Board(fen), move_stack() {}
		bool setFen(std::string_view fen){
			move_stack.clear();
			return Board::setFen(fen);
		}
		bool setEpd(std::string_view fen){
			move_stack.clear();
			return Board::setEpd(fen);
		}
		Position(const Position &other)
		{
			Board::operator=(other);
			move_stack = other.move_stack;
		}

		Position &operator=(const Position &other)
		{
			if (this != &other)
			{
				Board::operator=(other);
				move_stack = other.move_stack;
			}
			return *this;
		}

		void makeMove(const Move &move)
		{
			move_stack.push(move);
			Board::makeMove(move);
		}

		Move pop()
		{
			Move move = move_stack.pop();
			Board::unmakeMove(move);
			return move;
		}

		MoveStack move_stack;
	};
};
#define MAX_PLY 245
#define MAX_MATE 31999
#define MAX_SCORE_CP 31000
#define MATE(i) MAX_MATE-i
#define MATE_DISTANCE(i) (i - MAX_MATE)
int eval(chess::Position &board);
constexpr int16_t ASPIRATION_DELTA = 30;