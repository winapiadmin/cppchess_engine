#include "search.hpp"

namespace search
{
  // Transposition table
  TranspositionTable tt(20);
  // Principal variation
  chess::Move pv[256];

void updatePV(int ply, chess::Move best_move) {
  chess::Move newPV[256];
  newPV[ply] = best_move;

  int i = ply + 1;
  for (; i < 256 && pv[i] != chess::Move(); ++i)
    newPV[i] = pv[i];

  newPV[i] = chess::Move(); // null-terminate

  std::memcpy(pv + ply, newPV + ply, sizeof(chess::Move) * (i - ply + 1));
}

  int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth, int ply)
  {
    nodes++;
    auto ttEntry = tt.probe(board.hash());
    if (ttEntry)
    {
      if (ttEntry->depth() >= depth)
      {
        if (ttEntry->flag() == TTFlag::EXACT)
          return ttEntry->score();
        else if (ttEntry->flag() == TTFlag::LOWERBOUND)
          alpha = std::max(alpha, ttEntry->score());
        else if (ttEntry->flag() == TTFlag::UPPERBOUND)
          beta = std::min(beta, ttEntry->score());
        if (alpha >= beta)
          return ttEntry->score();
      }
    }
    if (ply == 0)
    {
      memset(pv, 0, sizeof(pv));
      nodes = 0;
      tt.clear_stats();
    }
    if (depth == 0)
      return quiescence(board, alpha, beta, ply);

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    if (moves.size() == 0)
    {
      if (board.inCheck())
        return MATE(ply);
      else
        return 0;
    }

    move_ordering::moveOrder(board, moves, ply);
    chess::Move bestmove = moves[0];
    bool foundPV = false;

    for (const auto &move : moves)
    {
      board.makeMove(move);
      int16_t score = -alphaBeta(board, -beta, -alpha, depth - 1, ply + 1);
      board.pop();

      if (score >= beta)
      {
        tt.store(board.hash(), move, score, depth, TTFlag::LOWERBOUND);
        return beta;
      }
      if (score > alpha)
      {
        alpha = score;
        bestmove = move;
        updatePV(ply, bestmove);  // Move + child PV
        foundPV = true;
      }
    }

    tt.store(board.hash(), bestmove, alpha, depth,
             foundPV ? TTFlag::EXACT : TTFlag::UPPERBOUND);
    return alpha;
  }

  int16_t quiescence(chess::Position &board, int16_t alpha, int16_t beta, int ply)
  {
    int16_t standPat = eval(board);
    if (standPat >= beta)
      return beta;
    if (standPat > alpha)
      alpha = standPat;

    chess::Movelist moves;
    chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(moves, board);
    move_ordering::moveOrder(board, moves, ply);

    for (const auto &move : moves)
    {
      board.makeMove(move);
      int16_t score = -quiescence(board, -beta, -alpha, ply + 1);
      board.pop();

      if (score >= beta)
        return beta;
      if (score > alpha)
        alpha = score;
    }
    return alpha;
  }

  void printPV(chess::Position &board)
  {
    std::cout << "Principal Variation: ";
    for (int i = 0; i < 256 && pv[i] != chess::Move(); i++)
      std::cout << pv[i] << " ";
    std::cout << std::endl;
  }
}
