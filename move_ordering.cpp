#include "move_ordering.hpp"
namespace move_ordering
{
  void moveOrder(chess::Position &board, chess::Movelist &moves, int ply)
  {

    const chess::Move prev = board.move_stack.empty() ? chess::Move() : board.move_stack.back();
    const int side = (int)board.sideToMove();

    for (int i = 0; i < moves.size(); i++) {
        chess::Move& move = moves[i]; // Get a reference to allow score modification
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

        move.setScore(score); // Use the public setter
    }

    // Sort moves in descending order of score
    std::sort(moves.begin(), moves.end(), [](const chess::Move& a, const chess::Move& b) {
        return a.score() > b.score(); // Use the public getter
    });
  }
}