#include "search.h"
#include "eval.h"
#include "timeman.h"
#include "uci.h"
#include "movepick.h"
#include <atomic>
#include <moves_io.h>
#include <position.h>
using namespace chess;
namespace engine
{
  TranspositionTable search::tt(16);
  std::atomic<bool> stopSearch{false};
  void search::stop()
  {
    tt.clear();
    stopSearch.store(true, std::memory_order_relaxed);
  }
  struct Session
  {
    timeman::TimeManagement tm;
    timeman::LimitsType tc;
    int seldepth = 0;
    uint64_t nodes = 0;
    chess::Move pv[MAX_PLY][MAX_PLY];
  };
  void update_pv(Move *pv, Move move, const Move *childPv)
  {

    for (*pv++ = move; childPv && *childPv != Move::none();)
      *pv++ = *childPv++;
    *pv = Move::none();
  }
  Value doSearch(Board &board, int depth, Value alpha, Value beta, Session &session, int ply = 0)
  {
    std::fill(std::begin(session.pv[ply + 1]), std::end(session.pv[ply + 1]),
              Move::none());
    if (session.tm.elapsed() >=
            session.tm.optimum() ||
        stopSearch.load(std::memory_order_relaxed))
      return VALUE_NONE;
    if (board.is_draw(3))
    {
      session.nodes++;
      session.pv[ply][0] = Move::none();
      return 0;
    }
    session.seldepth = std::max(session.seldepth, ply);
    uint64_t hash = board.hash();
    Move preferred = Move::none();
    if (TTEntry *entry = search::tt.lookup(hash))
    {
      preferred = Move(entry->getMove());
      if (entry->getDepth() >= depth)
      {
        Value ttScore = entry->getScore();
        TTFlag flag = entry->getFlag();

        if (flag == TTFlag::EXACT)
            return ttScore;

        if (flag == TTFlag::LOWERBOUND && ttScore >= beta)
            return ttScore;

        if (flag == TTFlag::UPPERBOUND && ttScore <= alpha)
            return ttScore;
      }
    }
    if (depth == 0)
    {
      session.nodes++;
      return eval::eval(board);
    }
    Value maxScore = -VALUE_INFINITE;
    Movelist moves;
    board.legals(moves);
    if (!moves.size())
    {
      session.pv[ply][0] = Move::none();
      return board.checkers() ? -MATE(ply) : 0;
    }
    movepick::orderMoves(board, moves, preferred, ply);
    for (Move move : moves)
    {

      board.doMove(move);

      Value childScore = doSearch(board, depth - 1, -beta, -alpha, session, ply + 1);

      board.undoMove();

      // ---- ABORT PROPAGATION ----
      if (childScore == VALUE_NONE)
        return VALUE_NONE;

      Value score = -childScore;

      if (score > maxScore)
      {
        maxScore = score;
        update_pv(session.pv[ply], move, session.pv[ply + 1]);
      }

      if (score > alpha){
          alpha = score;
          if (!board.isCapture(move))
              historyHeuristic[from][to] += depth * depth;
      }
      if (alpha >= beta)
      {
          if (!board.isCapture(move))
          {
              if (killerMoves[ply][0] != move)
              {
                  killerMoves[ply][1] = killerMoves[ply][0];
                  killerMoves[ply][0] = move;
              }
          }
      
          break;
      }
      
      if (session.tm.elapsed() >=
              session.tm.optimum() ||
          stopSearch.load(std::memory_order_relaxed))
        return VALUE_NONE;
    }

    if (maxScore != -VALUE_INFINITE)
      TTFlag flag;

      if (maxScore <= alphaOrig)
          flag = TTFlag::UPPERBOUND;
      else if (maxScore >= beta)
          flag = TTFlag::LOWERBOUND;
      else
          flag = TTFlag::EXACT;
      
      search::tt.store(hash, session.pv[ply][0], maxScore, depth, flag);
    }
    return maxScore;
  }
  void search::search(const chess::Board &board,
                      const timeman::LimitsType timecontrol)
  {
    static double originalTimeAdjust = -1;
    Session session;
    session.tc = timecontrol;
    session.tm.init(session.tc, board.sideToMove(), 0, originalTimeAdjust);
    InfoFull lastInfo{};
    chess::Move lastPV[MAX_PLY]{};
    for (int i = 1; i < timecontrol.depth; i++)
    {
      for (int i=0;i<64;i++)for (int j=0;j<64;j++)movepick::historyHeuristic[i][j]/=2;
      session.nodes = 0;
      auto board_ = board;
      Value score_ = doSearch(board_, i, -VALUE_INFINITE, VALUE_INFINITE, session);
      if (session.tm.elapsed() >=
              session.tm.optimum() ||
          stopSearch.load(std::memory_order_relaxed) || score_ == VALUE_NONE)
        break;
      InfoFull info{};
      info.depth = i;
      info.selDepth = session.seldepth;
      info.hashfull = tt.hashfull();
      info.nodes = session.nodes;
      info.nps = session.nodes * 1000 /
                 std::max(session.tm.elapsed(),
                          (timeman::TimePoint)1);
      info.timeMs = session.tm.elapsed();
      info.multiPV = 1;
      info.score = score_;
      std::string pv = "";
      for (Move *m = session.pv[0]; *m != Move::none(); m++)
        pv += chess::uci::moveToUci(*m, board.chess960()) + " ";
      info.pv = pv;
      report(info);
      lastInfo = info;
      std::copy(session.pv[0], &session.pv[0][MAX_PLY], lastPV);
    }
    if (lastPV[0].is_ok())
      report(chess::uci::moveToUci(lastPV[0]));
    else
    {
      // try to TT probe it
      TTEntry *entry = tt.lookup(board.hash());
      if (entry && entry->getMove() != Move::none().raw())
        report(chess::uci::moveToUci(Move(entry->getMove()), board.chess960()));
      else
      {
        Movelist moves;
        board.legals(moves);

        if (moves.size())
        {
          Board board_ = board;
          Move best = moves[0];
          Value bestScore = -VALUE_INFINITE;
          for (Move move : moves)
          {
            board_.doMove(move);
            Value score = -eval::eval(board_);
            if (score > bestScore)
            {
              bestScore = score;
              best = move;
            }
            board_.undoMove();
          }

          InfoFull info{};
          info.depth = 1;
          info.nodes = 1;
          info.score = 0;
          info.multiPV = 1;
          info.pv = chess::uci::moveToUci(best, board.chess960());
          report(info);

          report(chess::uci::moveToUci(best, board.chess960()));
        }
        else
        {
          report("0000");
        }
      }
    }
  }
} // namespace engine
