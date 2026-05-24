#include "movepick.h"
#include "eval.h"
#include "search.h"
#include <algorithm>
using namespace chess;
using engine::eval::piece_value;
namespace engine::movepick {

void orderMoves(chess::Board &board, chess::Movelist &moves, chess::Move ttMove,
                int ply, const engine::search::Session &session) {
  std::vector<std::pair<chess::Move, Value>> scoredMoves;
  scoredMoves.reserve(moves.size());

  for (const auto &move : moves) {
    Value score = 0;

    if (move == ttMove)
      score = 10000;
    else if (board.isCapture(move))
      score = ((move.typeOf() & EN_PASSANT) == 0
                   ? piece_value(board.at<PieceType>(move.to()))
                   : piece_value(PAWN)) *
                  10 -
              piece_value(board.at<PieceType>(move.from()));
    else if (move == session.killerMoves[ply][0])
      score = 8500;
    else if (move == session.killerMoves[ply][1])
      score = 8000;
    else
      score = session.historyHeuristic[move.from()][move.to()];

    scoredMoves.emplace_back(move, score);
  }

  std::stable_sort(
      scoredMoves.begin(), scoredMoves.end(),
      [](const auto &a, const auto &b) { return a.second > b.second; });

  for (size_t i = 0; i < scoredMoves.size(); ++i)
    moves[i] = scoredMoves[i].first;
}
} // namespace engine::movepick
