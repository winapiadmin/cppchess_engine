/**
 * @brief Reorders and scores moves for improved search efficiency in chess move ordering
 *
 * This function assigns scores to moves based on various heuristics to prioritize
 * potentially more promising moves during chess move search. Scoring considers:
 * - Captures (using Most Valuable Victim - Least Valuable Attacker)
 * - Counter moves
 * - Killer moves
 * - Historical move performance
 *
 * @param board The current chess board position
 * @param moves A list of moves to be scored and reordered
 * @param ply The current search depth/ply
 */
#include "move_ordering.hpp"
namespace move_ordering {
// Static Exchange Evaluation (actually quiescence search but captures only)
int16_t see(chess::Position &board, chess::Square target, int16_t gain_so_far) {
    int16_t gain = -MATE(0);
    chess::Movelist cap;
    chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(cap, board);
    chess::PieceType p = board.at(target).type();
    if (p == chess::PieceType::NONE) return 0;
    for (const auto &m : cap) {
        if (m.to() == target) {
            board.makeMove(m);
            gain = std::max(gain, (int16_t)-see(board, target, gain_so_far + piece_value(p)));
            board.pop();
        }
    }
    return gain != -MATE(0) ? std::max(gain, gain_so_far) : 0;
}
int16_t see(const chess::Position &a, chess::Square b) {
    chess::Position board = a;
    return see(board, b, 0);
}
void moveOrder(chess::Position &board, chess::Movelist &moves, int ply, chess::Move ttMove) {
    const chess::Move prev = board.move_stack.empty() ? chess::Move() : board.move_stack.back();
    const int side = (int)board.sideToMove();

    // Pre-calculate common values used in the loop
    constexpr int killer1_score = SCORE_KILLER;
    constexpr int killer2_score = SCORE_KILLER / 2;
    const chess::Move &killer1 = killerMoves[0][ply];
    const chess::Move &killer2 = killerMoves[1][ply];
    const chess::Move &counter = counterMove[prev.from().index()][prev.to().index()];

    for (int i = 0; i < moves.size(); i++) {
        chess::Move &move = moves[i];
        int score = 0;
        if (move == ttMove) score = 10000;
        // Capture scoring
        if (board.isCapture(move)) {
            // auto victim = board.at(move.to()).type();
            // auto attacker = board.at(move.from()).type();
            score += SCORE_CAPTURE + see(board, move.to(), 0);
        }

        // Counter move bonus (simple equality check)
        if (move == counter) {
            score += SCORE_COUNTER;
        }

        // Killer move bonus (simple equality checks)
        if (move == killer1)
            score += killer1_score;
        else if (move == killer2)
            score += killer2_score;

        // History heuristic
        score += history[side][move.from().index()][move.to().index()] * SCORE_HISTORY;

        // Promotion bonus
        if (move.typeOf() == chess::Move::PROMOTION) score += piece_value(move.promotionType());

        // Set the score
        move.setScore(score);
    }
    const int TOP_MOVES = 5;
    if (moves.size() > TOP_MOVES) {
        std::partial_sort(moves.begin(), moves.begin() + TOP_MOVES, moves.end(),
                          [](const chess::Move &a, const chess::Move &b) { return a.score() > b.score(); });
    } else {
        std::sort(moves.begin(), moves.end(),
                  [](const chess::Move &a, const chess::Move &b) { return a.score() > b.score(); });
    }
}
}  // namespace move_ordering
