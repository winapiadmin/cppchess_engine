#include "search.h"
#include "eval.h"
#include "movepick.h"
#include "timeman.h"
#include "uci.h"
#include <atomic>
#include <iostream>
#include <moves_io.h>
#include <position.h>
#include <printers.h>
using namespace chess;
namespace engine {
TranspositionTable search::tt(16);
std::atomic<bool> stopSearch{false};
void search::stop() { stopSearch.store(true, std::memory_order_relaxed); }
bool search::isStopped() { return stopSearch; }
struct Session {
  timeman::TimeManagement tm;
  timeman::LimitsType tc;
  int seldepth = 0;
  uint64_t nodes = 0;
  chess::Move pv[MAX_PLY][MAX_PLY];
};
namespace { // SF
void update_pv(Move *pv, Move move, const Move *childPv) {

  for (*pv++ = move; childPv && *childPv != Move::none();)
    *pv++ = *childPv++;
  *pv = Move::none();
}
// Adjusts a mate or TB score from "plies to mate from the root" to
// "plies to mate from the current position". Standard scores are unchanged.
// The function is called before storing a value in the transposition table.
Value value_to_tt(Value v, int ply) {
  return is_win(v) ? v + ply : is_loss(v) ? v - ply : v;
}

// Inverse of value_to_tt(): it adjusts a mate or TB score from the
// transposition table (which refers to the plies to mate/be mated from current
// position) to "plies to mate/be mated (TB win/loss) from the root". However,
// to avoid potentially false mate or TB scores related to the 50 moves rule and
// the graph history interaction, we return the highest non-TB score instead.
Value value_from_tt(Value v, int ply, int r50c) {

  if (!is_valid(v))
    return VALUE_NONE;

  // handle TB win or better
  if (is_win(v)) {
    // Downgrade a potentially false mate score
    if (v >= VALUE_MATE_IN_MAX_PLY && VALUE_MATE - v > 100 - r50c)
      return VALUE_TB_WIN_IN_MAX_PLY - 1;

    // Downgrade a potentially false TB score.
    if (VALUE_TB - v > 100 - r50c)
      return VALUE_TB_WIN_IN_MAX_PLY - 1;

    return v - ply;
  }

  // handle TB loss or worse
  if (is_loss(v)) {
    // Downgrade a potentially false mate score.
    if (v <= VALUE_MATED_IN_MAX_PLY && VALUE_MATE + v > 100 - r50c)
      return VALUE_TB_LOSS_IN_MAX_PLY + 1;

    // Downgrade a potentially false TB score.
    if (VALUE_TB + v > 100 - r50c)
      return VALUE_TB_LOSS_IN_MAX_PLY + 1;

    return v + ply;
  }

  return v;
}
} // namespace
Value qsearch(Board &board, Value alpha, Value beta, Session &session,
              int ply = 0) {
  session.nodes++;
  session.seldepth = std::max(session.seldepth, ply);
  int standPat = eval::eval(board);
  if (session.tm.elapsed() >= session.tm.optimum() ||
      stopSearch.load(std::memory_order_relaxed))
    return standPat;
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
Value doSearch(Board &board, int depth, Value alpha, Value beta,
               Session &session, int ply = 0) {
  // TLE or exceeded depth limit
  if (ply >= MAX_PLY - 1)
    return eval::eval(board);
  if (depth <= 0) {
    session.nodes++;
    return qsearch(board, alpha, beta, session, ply);
  }
  if (session.tm.elapsed() >= session.tm.optimum() ||
      stopSearch.load(std::memory_order_relaxed))
    return qsearch(board, alpha, beta, session, ply);
  Value alphaOrig = alpha;
  // Reset PV
  std::fill(std::begin(session.pv[ply]), std::end(session.pv[ply]),
            Move::none());
  std::fill(std::begin(session.pv[ply + 1]), std::end(session.pv[ply + 1]),
            Move::none());
  if (board.is_draw(3) || board.is_insufficient_material()) {
    session.nodes++;
    session.pv[ply][0] = Move::none();
    return 0;
  }
  session.seldepth = std::max(session.seldepth, ply);
  uint64_t hash = board.hash();
  Move preferred = Move::none();
  if (TTEntry *entry = search::tt.lookup(hash)) {
    if (entry->getDepth() >= depth) {
      Value ttScore =
          value_from_tt(entry->getScore(), ply, board.rule50_count());
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
    uint64_t hash_ = board.hash();
    board.doNullMove();
    Value score =
        doSearch(board, depth - 1 - R, -beta, -beta + 1, session, ply + 1);

    score = -score;
    board.undoMove();
    if (score >= beta)
      return score;
  }
  for (size_t i = 0; i < moves.size(); ++i) {
    Move move = moves[i];

    bool isCapture = board.isCapture(move);
    bool givesCheck = board.givesCheck(move) != CheckType::NO_CHECK;

    // --- LMR reduction ---
    int reduction = 0;
    if (i >= 3 && depth >= 3 && !isCapture && !givesCheck) {
      reduction = 1 + (int)(i / 6) + (depth / 8);

      // history heuristic: good moves get reduced less
      if (movepick::historyHeuristic[(int)move.from()][(int)move.to()] > 0)
        reduction--;

      reduction = std::max(0, reduction);
      reduction = std::min(reduction, depth - 2);
    }

    board.doMove(move);

    Value score;

    if (i == 0) {
      uint64_t hash_ = board.hash();
      // --- First move: full window (PVS root move) ---
      score = doSearch(board, depth - 1, -beta, -alpha, session, ply + 1);

      if (score == VALUE_NONE) {
        board.undoMove();
        return VALUE_NONE;
      }
      score = -score;
    } else {
      uint64_t hash_ = board.hash();
      // --- Null-window search (PVS + LMR) ---
      score = doSearch(board, depth - 1 - reduction, -alpha - 1, -alpha,
                       session, ply + 1);
      if (score == VALUE_NONE) {
        board.undoMove();
        return VALUE_NONE;
      }
      score = -score;
      // --- Re-search if it improves alpha ---
      if (score > alpha) {
        score = doSearch(board, depth - 1, -beta, -alpha, session, ply + 1);
        if (score == VALUE_NONE) {
          board.undoMove();
          return VALUE_NONE;
        }
        score = -score;
      }
    }

    board.undoMove();
    if (score > maxScore) {
      maxScore = score;
      update_pv(session.pv[ply], move, session.pv[ply + 1]);
    }

    if (score > alpha) {
      alpha = score;

      if (!isCapture)
        movepick::historyHeuristic[(int)move.from()][(int)move.to()] +=
            depth * depth;
    }

    if (alpha >= beta) {
      // killer moves
      if (!isCapture) {
        if (movepick::killerMoves[ply][0] != move) {
          movepick::killerMoves[ply][1] = movepick::killerMoves[ply][0];
          movepick::killerMoves[ply][0] = move;
        }
      }
      break;
    }

    if (session.tm.elapsed() >= session.tm.optimum() ||
        stopSearch.load(std::memory_order_relaxed))
      return maxScore;
  }

  if (maxScore != -VALUE_INFINITE) {
    TTFlag flag;

    if (maxScore <= alphaOrig)
      flag = TTFlag::UPPERBOUND;
    else if (maxScore >= beta)
      flag = TTFlag::LOWERBOUND;
    else
      flag = TTFlag::EXACT;

    search::tt.store(hash, session.pv[ply][0], value_to_tt(maxScore, ply),
                     depth, flag);
  }
  return maxScore;
}
void search::search(const chess::Board &board,
                    const timeman::LimitsType timecontrol) {
  stopSearch = false;
  static double originalTimeAdjust = -1;
  Session session;
  session.tc = timecontrol;
  session.tm.init(session.tc, board.side_to_move(), 0, originalTimeAdjust);
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
        stopSearch.load(std::memory_order_relaxed) || score_ == VALUE_NONE)
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
