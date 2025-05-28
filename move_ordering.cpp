/**
 * @file move_ordering.hpp
 * @brief Implements move ordering heuristics for chess search optimization.
 *
 * This module scores and reorders moves based on various heuristics:
 * - Transposition Table (TT) move prioritization
 * - Capture ordering using Static Exchange Evaluation (SEE)
 * - Counter move bonus
 * - Killer moves
 * - History heuristic
 * - Promotion bonuses
 *
 * See: https://www.chessprogramming.org/Move_Ordering
 */

#include "move_ordering.hpp"
#include <algorithm>

namespace move_ordering {

// Constants for scoring (tune these if needed)
constexpr int SCORE_TT = 10000;

// Recursive SEE implementation
// sideToMove: side currently to move in simulation
int16_t see(chess::Position &board, chess::Square target) {
    int16_t maxGain = -MATE(0);
    chess::Movelist captures;
    chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(captures, board);

    chess::PieceType victim = board.at(target).type();
    if (victim == chess::PieceType::NONE) return 0;

    bool foundCapture = false;
    for (const auto &move : captures) {
        if (move.to() == target && board.at(move.from()).color() == board.sideToMove()) {
            foundCapture = true;

            chess::PieceType attacker = board.at(move.from()).type();

            board.makeMove(move);
            int16_t score = piece_value(victim) - see(board, target);
            board.pop();

            if (score > maxGain)
                maxGain = score;
        }
    }
    return foundCapture ? maxGain : 0;
}


// Main move ordering function
void moveOrder(chess::Position &board, chess::Movelist &moves, int ply, chess::Move ttMove) {
    const chess::Move prevMove = board.move_stack.empty() ? chess::Move() : board.move_stack.back();
    const int side = static_cast<int>(board.sideToMove());

    // Retrieve killer and counter moves for this ply
    const chess::Move &killer1 = killerMoves[0][ply];
    const chess::Move &killer2 = killerMoves[1][ply];
    const chess::Move &counter = counterMove[prevMove.from().index()][prevMove.to().index()];

    for (auto &move : moves) {
        int score = 0;

        // TT move highest priority
        if (move == ttMove) {
            score = SCORE_TT;
        }

        // Captures scored by SEE + base capture score
        else if (board.isCapture(move)) {
            score = SCORE_CAPTURE + see(board, move.to());
        }

        // Counter move bonus
        if (move == counter) {
            score += SCORE_COUNTER;
        }

        // Killer moves bonus
        if (move == killer1) {
            score += SCORE_KILLER;
        } else if (move == killer2) {
            score += SCORE_KILLER / 2;
        }

        // History heuristic contribution
        score += history[side][move.from().index()][move.to().index()] * SCORE_HISTORY;

        // Promotion bonus (e.g., queen > knight)
        if (move.typeOf() == chess::Move::PROMOTION) {
            score += piece_value(move.promotionType());
        }

        move.setScore(score);
    }

    constexpr int TOP_MOVES = 5;
    if (moves.size() > TOP_MOVES) {
        std::partial_sort(moves.begin(), moves.begin() + TOP_MOVES, moves.end(),
                          [](const chess::Move &a, const chess::Move &b) { return a.score() > b.score(); });
    } else {
        std::sort(moves.begin(), moves.end(),
                  [](const chess::Move &a, const chess::Move &b) { return a.score() > b.score(); });
    }
}

}  // namespace move_ordering
