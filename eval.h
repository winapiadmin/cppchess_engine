#pragma once
#include "chess.hpp"
#include <map>
#include <vector>
#include <cstdint>
#include <cstring>
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
int16_t eval(const chess::Position &board);
constexpr int16_t ASPIRATION_DELTA = 30;
int16_t piece_value(chess::PieceType p);
int16_t passed(const chess::Position &pos);
void trace(const chess::Position& pos);