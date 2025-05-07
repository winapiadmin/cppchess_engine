// File: move_ordering.hpp
#pragma once

#include "chess.hpp"
#include "eval.h"
#include <vector>
#include <algorithm>

namespace move_ordering {

constexpr int SCORE_CAPTURE = 10000;
constexpr int SCORE_PROMOTION = 9000;
constexpr int SCORE_KILLER = 8000;
constexpr int SCORE_COUNTER = 7000;
constexpr int SCORE_HISTORY = 1;

inline int mvvLva(chess::PieceType victim, chess::PieceType attacker) {
    static const int pieceValue[] = {0, 100, 300, 325, 500, 900, 0}; // EMPTY, P, N, B, R, Q, K
    return pieceValue[(int)victim] * 10 - pieceValue[(int)attacker];
}

inline chess::Move counterMove[64][64];
inline int16_t history[2][64][64] = {};
inline chess::Move killerMoves[2][128] = {};
void moveOrder(chess::Position& board, chess::Movelist& moves, int ply);

inline void updateKillers(chess::Move move, int ply) {
    if (killerMoves[0][ply] != move) {
        killerMoves[1][ply] = killerMoves[0][ply];
        killerMoves[0][ply] = move;
    }
}

} // namespace move_ordering
