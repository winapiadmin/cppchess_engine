#include "search.hpp"

namespace search
{
  // Transposition table
  TranspositionTable tt(20);

  int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth, int ply, PV *pv)
  {
    PV _pv={0};
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
      int16_t score = -alphaBeta(board, -beta, -alpha, depth - 1, ply + 1, &_pv);
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
        pv->argmove[0] = move;

        memcpy(pv->argmove + 1, _pv.argmove, _pv.cmove * sizeof(chess::Move));

        pv->cmove = _pv.cmove + 1;
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
  PV pv;
  int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth)
  {
    memset(&pv, 0, sizeof(pv));
    pv.cmove = 0;
    nodes = 0;
    tt.clear_stats();
    return alphaBeta(board, alpha, beta, depth, 0, &pv);
  }
  void printPV(chess::Position &board)
  {
    std::cout << "Principal Variation: ";
    for (int i = 0; i < pv.cmove && pv.argmove[i] != chess::Move(); i++)
      std::cout << pv.argmove[i] << " ";
    std::cout << std::endl;
  }
}
