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

inline void moveOrder(chess::Position& board, chess::Movelist& moves, int ply) {
    static thread_local std::vector<std::pair<chess::Move, int>> scoredMoves;
    scoredMoves.clear();
    scoredMoves.reserve(moves.size());
    const chess::Move prev = board.move_stack.empty() ? chess::Move() : board.move_stack.back();
    const int side = (int)board.sideToMove();

    for (int i = 0; i < moves.size(); i++) {
        const chess::Move& move = moves[i];
        int score = 0;

        if (board.isCapture(move)) {
            auto victim = board.at(move.to()).type();
            auto attacker = board.at(move.from()).type();
            score += SCORE_CAPTURE + mvvLva(victim, attacker);
        }

        if (move == counterMove[prev.from().index()][prev.to().index()]) {
            score += SCORE_COUNTER;
        }

        if (move == killerMoves[0][ply]) score += SCORE_KILLER;
        else if (move == killerMoves[1][ply]) score += SCORE_KILLER / 2;

        score += history[side][move.from().index()][move.to().index()] * SCORE_HISTORY;

        scoredMoves.push_back({move, score});
    }

    std::sort(scoredMoves.begin(), scoredMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    moves.clear();
    for (const auto& [move, _] : scoredMoves) {
        moves.add(move);
    }
}

inline void updateKillers(chess::Move move, int ply) {
    if (killerMoves[0][ply] != move) {
        killerMoves[1][ply] = killerMoves[0][ply];
        killerMoves[0][ply] = move;
    }
}

} // namespace move_ordering
