#include "search.hpp"

namespace search {

TranspositionTable tt(20); // 16MB
chess::Move PV[128];
int pvLength = 0;
const auto INVALID_MOVE = chess::Move();

void clearPV() {
  std::memset(PV, 0, sizeof(PV));
  pvLength = 0;
}

void printPV(chess::Position &board) {
  std::cout << "PV: ";
  for (int i = 0; i < pvLength && PV[i] != INVALID_MOVE; ++i)
    std::cout << PV[i] << " ";
  std::cout << std::endl;
}

int16_t quiescence(chess::Position &board, int16_t alpha, int16_t beta,
                   int ply) {
  nodes++;
  int16_t standPat = eval(board);

  if (standPat >= beta)
    return beta;
  if (standPat > alpha)
    alpha = standPat;

  chess::Movelist moves;
  chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(moves,
                                                                   board);
  move_ordering::scoreMoves(board, moves, ply);

  for (auto &move : moves) {
    board.makeMove(move);
    int16_t score = -quiescence(board, -beta, -alpha, ply + 1);
    board.unmakeMove(move);

    if (score >= beta)
      return beta;
    if (score > alpha)
      alpha = score;
  }

  return alpha;
}

void updatePV(int ply, chess::Move move) {
  for (int j = ply; j < 128; ++j)
    PV[j] = INVALID_MOVE;
  PV[ply] = move;
  pvLength = 0;
  while (pvLength < 128 && PV[pvLength] != INVALID_MOVE)
    ++pvLength;
}

int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta,
                  int depth, int ply) {
  ++nodes;

  auto hash = board.hash();
  if (TTEntry *entry = tt.probe(hash)) {
    if (entry->depth() >= depth) {
      if (entry->flag() == 0) return entry->score();
      if (entry->flag() == 1 && entry->score() >= beta) return entry->score();
      if (entry->flag() == 2 && entry->score() <= alpha) return entry->score();
    }
  }

  if (!board.inCheck() && depth >= 3) {
    board.makeNullMove();
    int16_t nullScore = -alphaBeta(board, -beta, -beta + 1, depth - 3, ply + 1);
    board.unmakeNullMove();

    if (nullScore >= beta) {
      return beta; // Null move cutoff
    }
  }

  if (!board.inCheck() && depth <= 3) {
    const int RFP_MARGIN[] = {0, 200, 150, 100};
    int16_t standPat = eval(board);
    if (standPat + RFP_MARGIN[depth] <= alpha)
      return alpha;
  }

  chess::Movelist moves;
  chess::movegen::legalmoves(moves, board);

  if (moves.empty()) {
    return board.inCheck() ? MATE(ply) : 0;
  }

  if (ply == 0) clearPV();
  if (depth == 0) return quiescence(board, alpha, beta, ply);

  move_ordering::scoreMoves(board, moves, ply);
  chess::Move bestMove;
  int16_t bestScore = -MATE(0);
  int moveCount = 0;

  for (auto &move : moves) {
    ++moveCount;

    board.makeMove(move);
    int newDepth = depth - 1;

    bool doLMR = depth >= 3 && moveCount > 3 && !board.inCheck();
    if (doLMR && newDepth > 1) newDepth--;

    if (newDepth <= 0) newDepth = 1;

    int16_t score = -alphaBeta(board, -alpha - 1, -alpha, newDepth, ply + 1);
    board.pop();

    if (score > bestScore) {
      if (score > alpha && score < beta && doLMR && depth > 1) {
        board.makeMove(move);
        score = -alphaBeta(board, -beta, -alpha, depth - 1, ply + 1);
        board.pop();
      }

      bestScore = score;
      bestMove = move;
      updatePV(ply, bestMove);
    }

    if (score >= beta) {
      tt.store(hash, move, beta, depth, 1);
      move_ordering::updateKillers(move, ply);
      return beta;
    }

    if (score > alpha) {
      alpha = score;
    }
  }

  uint8_t flag = (bestScore <= alpha) ? 2 : (bestScore >= beta) ? 1 : 0;
  tt.store(board.hash(), bestMove, bestScore, depth, flag);

  if (ply == 0) {
    pvLength = 0;
    while (pvLength < 128 && PV[pvLength] != INVALID_MOVE)
      ++pvLength;
  }

  return bestScore;
}

} // namespace search
