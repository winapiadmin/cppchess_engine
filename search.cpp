#include "search.h"
#include "eval.h"
#include "movepick.h"
#include "timeman.h"
#include "uci.h"
#include <atomic>
#include <moves_io.h>
#include <position.h>
#include <printers.h>
using namespace chess;
namespace engine {
TranspositionTable search::tt(16);
std::atomic<bool> stopSearch{false};
void search::stop() { stopSearch.store(true, std::memory_order_relaxed); }
struct Session {
  timeman::TimeManagement tm;
  timeman::LimitsType tc;
  int seldepth = 0;
  uint64_t nodes = 0;
  chess::Move pv[MAX_PLY][MAX_PLY];
};
void update_pv(Move *pv, Move move, const Move *childPv) {

  for (*pv++ = move; childPv && *childPv != Move::none();)
    *pv++ = *childPv++;
  *pv = Move::none();
}
Value qsearch(Board &board, Value alpha, Value beta, Session &session,
              int ply = 0) {
  if (session.tm.elapsed() >= session.tm.optimum() ||
      stopSearch.load(std::memory_order_relaxed))
    return VALUE_NONE;
  session.nodes++;
  session.seldepth = std::max(session.seldepth, ply);
  int standPat = eval::eval(board);
  Value maxScore = standPat;
  if (maxScore >= beta)
    return maxScore;
  if (maxScore > alpha)
    alpha = maxScore;
  Movelist moves;
  board.legals<MoveGenType::CAPTURE>(moves);
  for (Move move : moves) {
    board.doMove(move);
    Value score = qsearch(board, -beta, -alpha, session, ply + 1);
    board.undoMove();
    if (score == VALUE_NONE)
      return VALUE_NONE;
    score = -score;
    if (score >= beta)
      return score;
    if (score > maxScore)
      maxScore = score;
    if (score > alpha)
      alpha = score;
  }
  return maxScore;
}
Value doSearch(Board board, int depth, Value alpha, Value beta,
               Session &session, int ply = 0) {
  if (ply >= MAX_PLY - 1)
    return eval::eval(board);
  Value alphaOrig = alpha;
  std::fill(std::begin(session.pv[ply]), std::end(session.pv[ply]),
            Move::none());
  std::fill(std::begin(session.pv[ply + 1]), std::end(session.pv[ply + 1]),
            Move::none());
  if (session.tm.elapsed() >= session.tm.optimum() ||
      stopSearch.load(std::memory_order_relaxed))
    return VALUE_NONE;
  if (board.is_draw(3)) {
    session.nodes++;
    session.pv[ply][0] = Move::none();
    return 0;
  }
  session.seldepth = std::max(session.seldepth, ply);
  uint64_t hash = board.hash();
  Move preferred = Move::none();
  if (TTEntry *entry = search::tt.lookup(hash)) {
    if (entry->getDepth() >= depth) {
      Value ttScore = entry->getScore();
      TTFlag flag = entry->getFlag();

      if (flag == TTFlag::EXACT) {
        session.pv[ply][0] = Move(entry->getMove());
        session.pv[ply][1] = Move::none();
        return ttScore;
      }

      if (flag == TTFlag::LOWERBOUND && ttScore >= beta) {
        session.pv[ply][0] = Move(entry->getMove());
        session.pv[ply][1] = Move::none();
        return ttScore;
      }

      if (flag == TTFlag::UPPERBOUND && ttScore <= alpha) {
        session.pv[ply][0] = Move(entry->getMove());
        session.pv[ply][1] = Move::none();
        return ttScore;
      }
    }
    preferred = Move(entry->getMove());
  }
  if (depth == 0) {
    return qsearch(board, alpha, beta, session, ply);
  }
  Value maxScore = -VALUE_INFINITE;
  Movelist moves;
  board.legals(moves);
  if (!moves.size()) {
    session.pv[ply][0] = Move::none();
    return board.checkers() ? -MATE(ply) : 0;
  }
  movepick::orderMoves(board, moves, preferred, ply);
  if (bool useNMP = depth >= 3 && !board.checkers() && ply > 0) {
    int R = 2 + depth / 6;
    board.doNullMove();
    Value score =
        doSearch(board, depth - 1 - R, -beta, -beta + 1, session, ply + 1);

    if (score == VALUE_NONE) {
      board.undoMove();
      return VALUE_NONE;
    }
    score = -score;
    board.undoMove();
    if (score >= beta)
      return score;
  }
  for (size_t i = 0; i < moves.size(); ++i) {
    Move move = moves[i];
    int reduction = (i >= 3 && depth >= 3 && !board.isCapture(move)) ? 1 : 0;
    board.doMove(move);
    Value childScore = doSearch(board, depth - 1 - reduction, -alpha - 1, -alpha, session, ply + 1);
    if (childScore == VALUE_NONE){
      board.undoMove();
      return VALUE_NONE;
    }
    Value score = -childScore;
    if (reduction > 0 && score > alpha) {
      childScore = doSearch(board, depth - 1, -beta, -alpha, session, ply + 1);
      board.undoMove();
      if (childScore == VALUE_NONE) return VALUE_NONE;
      score = -childScore;
    }
    else
      board.undoMove();
      
    if (score > maxScore) {
      maxScore = score;
      update_pv(session.pv[ply], move, session.pv[ply + 1]);
    }

    if (score > alpha) {
      alpha = score;
      if (!board.isCapture(move))
        movepick::historyHeuristic[(int)move.from()][(int)move.to()] +=
            depth * depth;
    }
    if (alpha >= beta) {
      if (!board.isCapture(move)) {
        if (movepick::killerMoves[ply][0] != move) {
          movepick::killerMoves[ply][1] = movepick::killerMoves[ply][0];
          movepick::killerMoves[ply][0] = move;
        }
      }

      break;
    }

    if (session.tm.elapsed() >= session.tm.optimum() ||
        stopSearch.load(std::memory_order_relaxed))
      return VALUE_NONE;
  }

  if (maxScore != -VALUE_INFINITE) {
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
                    const timeman::LimitsType timecontrol) {
  stopSearch = false;
  static double originalTimeAdjust = -1;
  Session session;
  session.tc = timecontrol;
  session.tm.init(session.tc, board.sideToMove(), 0, originalTimeAdjust);
  InfoFull lastInfo{};
  chess::Move lastPV[MAX_PLY]{};
  for (int i = 1; i < timecontrol.depth; i++) {
    for (int _ = 0; _ < 64; _++)
      for (int j = 0; j < 64; j++) {
        movepick::historyHeuristic[_][j] /= 2;
        // since MAX_PLY=64
        session.pv[_][j] = Move::none();
      }
    auto board_ = board;
    Value score_ =
        doSearch(board_, i, -VALUE_INFINITE, VALUE_INFINITE, session);
    if (session.tm.elapsed() >= session.tm.optimum() ||
        stopSearch.load(std::memory_order_relaxed) || abs(score_) == VALUE_NONE)
      break;
    InfoFull info{};
    info.depth = i;
    info.selDepth = session.seldepth;
    info.hashfull = tt.hashfull();
    info.nodes = session.nodes;
    info.nps = session.nodes * 1000 /
               std::max(session.tm.elapsed(), (timeman::TimePoint)1);
    info.timeMs = session.tm.elapsed();
    info.multiPV = 1;
    info.score = score_;
    TTEntry *entry = tt.lookup(board.hash());
    if (entry)
      switch (entry->getFlag()) {
      case LOWERBOUND:
        info.bound = "lowerbound";
        break;
      case UPPERBOUND:
        info.bound = "upperbound";
        break;
      default:
        break;
      }
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
  else {
    // try to TT probe it
    TTEntry *entry = tt.lookup(board.hash());
    if (entry && entry->getMove() != Move::none().raw())
      report(chess::uci::moveToUci(Move(entry->getMove()), board.chess960()));
    else {
      Movelist moves;
      board.legals(moves);

      if (moves.size()) {
        Board board_ = board;
        Move best = moves[0];
        Value bestScore = -VALUE_INFINITE;
        for (Move move : moves) {
          board_.doMove(move);
          Value score = -eval::eval(board_);
          if (score > bestScore) {
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
        info.pv = std::string(chess::uci::moveToUci(best, board.chess960()));
        report(info);

        report(chess::uci::moveToUci(best, board.chess960()));
      } else {
        report("0000");
      }
    }
  }
}
} // namespace engine
