#include "search.h"
#include "eval.h"
#include "movepick.h"
#include "tb.h"
#include "timeman.h"
#include "uci.h"
#include <atomic>
#include <iostream>
#include <moves_io.h>
#include <position.h>
#include <printers.h>
#include <sstream>
#include <unordered_set>
using namespace chess;
namespace engine::search {
TranspositionTable tt(16);
std::atomic<bool> stopSearch{ false };
void stop() { stopSearch.store(true, std::memory_order_relaxed); }
bool isStopped() { return stopSearch.load(std::memory_order_relaxed); }
namespace {
void update_pv(Move *pv, Move move, const Move *childPv) {
    for (*pv++ = move; childPv && *childPv != Move::none();)
        *pv++ = *childPv++;
    *pv = Move::none();
}
// Adjusts a mate or TB score from "plies to mate from the root" to
// "plies to mate from the current position". Standard scores are unchanged.
// The function is called before storing a value in the transposition table.
Value value_to_tt(Value v, int ply) { return is_win(v) ? v + ply : is_loss(v) ? v - ply : v; }

// Inverse of value_to_tt(): it adjusts a mate or TB score from the transposition
// table (which refers to the plies to mate/be mated from current position) to
// "plies to mate/be mated (TB win/loss) from the root". However, to avoid
// potentially false mate or TB scores related to the 50 moves rule and the
// graph history interaction, we return the highest non-TB score instead.
Value value_from_tt(Value v, int ply, int r50c) {

    if (!is_valid(v))
        return VALUE_NONE;

    // handle TB win or better
    if (is_win(v)) {
        // Downgrade a potentially false mate score
        if (is_mate(v) && VALUE_MATE - v > 100 - r50c)
            return VALUE_TB_WIN_IN_MAX_PLY - 1;

        // Downgrade a potentially false TB score.
        if (VALUE_TB - v > 100 - r50c)
            return VALUE_TB_WIN_IN_MAX_PLY - 1;

        return v - ply;
    }

    // handle TB loss or worse
    if (is_loss(v)) {
        // Downgrade a potentially false mate score.
        if (is_mated(v) && VALUE_MATE + v > 100 - r50c)
            return VALUE_TB_LOSS_IN_MAX_PLY + 1;

        // Downgrade a potentially false TB score.
        if (VALUE_TB + v > 100 - r50c)
            return VALUE_TB_LOSS_IN_MAX_PLY + 1;

        return v + ply;
    }

    return v;
}
} // namespace
Value qsearch(Board &board, Value alpha, Value beta, search::Session &session, int ply) {
    session.nodes++;
    session.qnodes++;
    session.seldepth = std::max(session.seldepth, ply);
    if (((session.nodes & 2047) == 0 && session.tm.elapsed() >= session.tm.optimum()) ||
        stopSearch.load(std::memory_order_relaxed))
        return VALUE_NONE;

    bool inCheck = board.is_check();
    if (board.is_draw(3) || board.is_insufficient_material())
        return VALUE_DRAW;

    Move ttMove = Move::none();
    TTEntry *entry = search::tt.lookup(board.hash());
    Value alphaOrig = alpha;
    if (entry) {
        session.ttHits++;
        Value ttScore = value_from_tt(entry->getScore(), ply, board.rule50_count());

        if (entry->getFlag() == EXACT) {
            session.ttCutoffs++;
            return ttScore;
        }
        if (entry->getFlag() == LOWERBOUND && ttScore >= beta) {
            session.ttCutoffs++;
            return ttScore;
        }
        if (entry->getFlag() == UPPERBOUND && ttScore <= alpha) {
            session.ttCutoffs++;
            return ttScore;
        }
        if (entry->getFlag() == LOWERBOUND)
            alpha = std::max(alpha, ttScore);
        else if (entry->getFlag() == UPPERBOUND)
            beta = std::min(beta, ttScore);
        if (alpha >= beta) {
            session.ttCutoffs++;
            return ttScore;
        }
        ttMove = Move(entry->getMove());
    }

    Movelist moves;

    if (inCheck)
        board.legals(moves);
    else
        board.legals<MoveGenType::CAPTURE>(moves);
    Value best = -VALUE_INFINITE;
    Value standPat = VALUE_NONE;

    if (!inCheck) {
        standPat = eval::eval(board);

        if (standPat >= beta)
            return standPat;

        if (standPat > alpha)
            alpha = standPat;

        best = standPat;
    }

    if (!moves.size())
        return inCheck ? mated_in(ply) : best;
    if (ply >= MAX_PLY - 1)
        return best;

    movepick::orderMoves(board, moves, ttMove, ply, session, Move::none());
    int movesSearched = 0;
    for (Move move : moves) {
        bool isCapture = board.isCapture(move);
        bool givesCheck = board.givesCheck(move) != CheckType::NO_CHECK;

        if (!is_loss(best)) {
            if (!isCapture && !givesCheck)
                continue;
            if (isCapture && !givesCheck && move.type_of() != PROMOTION) {
                Value capturedValue =
                    move.type_of() == EN_PASSANT ? eval::piece_value(PAWN) : eval::piece_value(board.at<PieceType>(move.to()));
                if (standPat + capturedValue + 200 < alpha)
                    continue;
                if (movepick::see(board, move) < 0)
                    continue;
            }
        }

        board.doMove(move);

        Value score = -qsearch(board, -beta, -alpha, session, ply + 1);

        board.undoMove();

        if (score == VALUE_NONE)
            return VALUE_NONE;

        if (score > best) {
            ttMove = move;
            best = score;
        }
        if (score > alpha)
            alpha = score;
        if (alpha >= beta)
            break;

        movesSearched++;
    }
    TTFlag flag = best >= beta ? LOWERBOUND : best <= alphaOrig ? UPPERBOUND : EXACT;
    tt.store(board.hash(), ttMove, value_to_tt(best, ply), 0, flag);
    return best;
}
Value doSearch(
    Board &board, int depth, Value alpha, Value beta, search::Session &session, int ply = 0, Move prevMove = Move::none()) {
    session.nodes++;
    session.seldepth = std::max(session.seldepth, ply);
    if (ply >= MAX_PLY - 1)
        return eval::eval(board);
    if (depth <= 0)
        return qsearch(board, alpha, beta, session, ply);
    if (((session.nodes & 2047) == 0 && session.tm.elapsed() >= session.tm.optimum()) ||
        stopSearch.load(std::memory_order_relaxed))
        return VALUE_NONE;

    bool inCheck = board.is_check();

    alpha = std::max(mated_in(ply), alpha);
    beta = std::min(mate_in(ply + 1), beta);
    if (alpha >= beta)
        return alpha;

    session.pv[ply][0] = Move::none();
    if (board.is_draw(3) || board.is_insufficient_material())
        return VALUE_DRAW;

    Value alphaOrig = alpha;
    uint64_t hash = board.hash();
    Move ttMove = Move::none();
    Value staticEval = eval::eval(board);

    TTEntry *entry = search::tt.lookup(hash);
    if (entry) {
        if (entry->getDepth() >= depth) {
            session.ttHits++;
            Value ttScore = value_from_tt(entry->getScore(), ply, board.rule50_count());
            TTFlag flag = entry->getFlag();

            if (flag == EXACT && ply > 0) {
                session.ttCutoffs++;
                session.pv[ply][0] = Move(entry->getMove());
                session.pv[ply][1] = Move::none();
                return ttScore;
            }
            if (flag == LOWERBOUND && ttScore >= beta && ply > 0) {
                session.ttCutoffs++;
                session.pv[ply][0] = Move(entry->getMove());
                session.pv[ply][1] = Move::none();
                return ttScore;
            }
            if (flag == UPPERBOUND && ttScore <= alpha && ply > 0) {
                session.ttCutoffs++;
                session.pv[ply][0] = Move(entry->getMove());
                session.pv[ply][1] = Move::none();
                return ttScore;
            }
        }
        ttMove = Move(entry->getMove());
    }

    // Reverse futility pruning: if eval is well above beta, prune
    if (!inCheck && ply > 0 && depth <= 3 && staticEval - 150 * depth >= beta && !is_win(beta) &&
        std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY)
        return staticEval;

    // Razoring: if eval is far below alpha, try qsearch to verify
    if (depth <= 2 && !inCheck && ply > 0 && !is_win(alpha)) {
        Value razorMargin = Value(256 + 100 * depth);
        if (staticEval + razorMargin < alpha) {
            Value v = qsearch(board, alpha - 1, alpha, session, ply);
            if (v == VALUE_NONE)
                return VALUE_NONE;
            if (v < alpha && std::abs(v) < VALUE_TB_WIN_IN_MAX_PLY)
                return v;
        }
    }

    // Null move pruning (skip when a mate threat is possible)
    if (depth >= 3 && !inCheck && ply > 0 && staticEval >= beta && !is_win(beta)) {
        int R = 2 + depth / 6; // + std::min(2, depth / 10);
        board.doNullMove();
        Value score = doSearch(board, depth - 1 - R, -beta, -beta + 1, session, ply + 1, Move::none());
        board.undoMove();
        if (score == VALUE_NONE)
            return VALUE_NONE;
        score = -score;
        if (score >= beta) {
            session.nullCutoffs++;
            return score;
        }
    }

    // ProbCut: if eval is well above beta, verify with reduced-depth search
    if (depth >= 5 && !inCheck && !is_win(beta) && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY) {
        Value probCutMargin = Value(200 + 100 * (depth - 5));
        if (staticEval >= beta + probCutMargin) {
            Value v = doSearch(board, depth - 2, beta - 1, beta, session, ply, prevMove);
            if (v == VALUE_NONE)
                return VALUE_NONE;
            if (v >= beta)
                return v;
        }
    }

    // Existing static null-move / futility pruning
    if (!inCheck && staticEval < alpha - 512 - 293 * depth * depth) {
        Value value = qsearch(board, alpha - 1, alpha, session, ply);
        if (value == VALUE_NONE)
            return VALUE_NONE;
        if (value < alpha && std::abs(value) < VALUE_TB_WIN_IN_MAX_PLY)
            return value;
    }

    // Tablebase probing (unchanged)
    if (ply != 0) {
        if (popcount(board.occ()) <= 7 && board.castlingRights() == NO_CASTLING) {
            int wdl = engine::tb::probe_wdl(board);
            if (wdl != engine::tb::TB_ERROR) {
                session.tbHits++;

                int drawScore = 1;

                Value tbValue = VALUE_TB - ply;

                Value value = wdl < -drawScore ? -tbValue : wdl > drawScore ? tbValue : VALUE_DRAW + 2 * wdl * drawScore;
                TTFlag b = wdl < -drawScore ? UPPERBOUND : wdl > drawScore ? LOWERBOUND : EXACT;
                if (b == EXACT || (b == LOWERBOUND ? value >= beta : value <= alpha)) {
                    tt.store(hash, Move::none(), value_to_tt(value, ply), std::min(MAX_PLY - 1, depth + 6), b);
                    return value;
                }

                if (b == LOWERBOUND)
                    alpha = std::max(alpha, value);
                if (alpha >= beta)
                    return alpha;
            }
        }
    }

    // Internal Iterative Deepening (IID): get a TT move when we don't have one
    if (depth >= 8 && ttMove == Move::none() && !inCheck && alpha != beta - 1) {
        int d = std::max(2, depth - 2 - depth / 4);
        Value v = doSearch(board, d, alpha, beta, session, ply, prevMove);
        if (v == VALUE_NONE)
            return VALUE_NONE;
        if (TTEntry *e = search::tt.lookup(hash))
            ttMove = Move(e->getMove());
    }
    Movelist moves;
    board.legals(moves);
    if (!moves.size()) {
        session.pv[ply][0] = Move::none();
        return board.checkers() ? mated_in(ply) : 0;
    }
    movepick::orderMoves(board, moves, ttMove, ply, session, prevMove);

    // Singular Extension: extend TT move when it dominates all others
    int singularExt = 0;
    if (depth >= 12 && ttMove.is_ok() && !inCheck && ply > 0 && entry && entry->getDepth() >= depth - 4 &&
        entry->getFlag() != UPPERBOUND && alpha != beta - 1 && !is_win(beta)) {
        Value sBeta = std::max(staticEval - 2 * depth, Value(-VALUE_MATE));
        int r = std::max(2, depth / 4);
        if (moves.size() <= 5) {
            bool singular = true;
            for (size_t si = 0; si < moves.size() && singular; ++si) {
                if (ply == 0 && !session.tc.searchmoves.empty() &&
                    std::find(session.tc.searchmoves.begin(),
                              session.tc.searchmoves.end(),
                              chess::uci::moveToUci(moves[si], board.chess960())) == session.tc.searchmoves.end())
                    continue;
                if (moves[si] == ttMove)
                    continue;
                board.doMove(moves[si]);
                Value v = doSearch(board, r, -sBeta, -sBeta + 1, session, ply + 1, moves[si]);
                board.undoMove();
                if (v == VALUE_NONE) {
                    singular = false;
                    break;
                }
                v = -v;
                if (v >= sBeta)
                    singular = false;
            }
            if (singular)
                singularExt = 1;
        }
    }

    Value maxScore = -VALUE_INFINITE;
    int movesSearched = 0;

    for (size_t i = 0; i < moves.size(); ++i) {
        Move move = moves[i];
        if (ply == 0 && !session.tc.searchmoves.empty() &&
            std::find(session.tc.searchmoves.begin(),
                      session.tc.searchmoves.end(),
                      chess::uci::moveToUci(move, board.chess960())) == session.tc.searchmoves.end())
            continue;

        bool isCapture = board.isCapture(move);
        bool givesCheck = board.givesCheck(move) != CheckType::NO_CHECK;

        // Futility pruning at shallow depths
        if (!inCheck && !isCapture && !givesCheck && depth <= 2 && ply > 0 && movesSearched > 0) {
            Value margin = Value(128 + 128 * depth);
            if (staticEval + margin <= alpha && std::abs(alpha) < VALUE_TB_WIN_IN_MAX_PLY)
                continue;
        }

        // Late move pruning at very shallow depths
        if (!inCheck && !isCapture && !givesCheck && depth <= 2 && movesSearched > 3 + 2 * depth &&
            std::abs(alpha) < VALUE_TB_WIN_IN_MAX_PLY)
            continue;

        // SEE pruning for losing captures at shallow depths
        if (!inCheck && isCapture && !givesCheck && depth <= 2 && movesSearched > 0 && move.type_of() != PROMOTION &&
            std::abs(alpha) < VALUE_TB_WIN_IN_MAX_PLY) {
            if (movepick::see(board, move) < 0)
                continue;
        }

        // LMR reduction
        int reduction = 0;
        if (movesSearched >= 2 && depth >= 3) {
            if (!isCapture && !givesCheck) {
                reduction = 1 + movesSearched / 5 + depth / 7;
                int history = session.historyHeuristic[(int)move.from()][(int)move.to()];
                if (history > 0)
                    reduction--;
                else if (history < 0)
                    reduction++;
                if (move == session.killerMoves[ply][0] || move == session.killerMoves[ply][1])
                    reduction--;
                if (prevMove.is_ok() && move == session.counterMoves[prevMove.from_to()])
                    reduction--;
                if (staticEval + 50 < alphaOrig)
                    reduction++;
                else if (staticEval - 50 >= alphaOrig)
                    reduction--;
            } else if (movesSearched >= 6) {
                reduction = 1 + movesSearched / 8;
            }
            reduction = std::clamp(reduction, 1, depth - 2);
        }

        int ext = 0;
        if (singularExt && movesSearched == 0)
            ext = 1;
        if (ext == 0 && moves.size() == 1)
            ext = 1;
        if (ext == 0 && isCapture && movesSearched == 0)
            ext = 1;
        board.doMove(move);

        Value score;

        if (movesSearched == 0 || reduction == 0) {
            score = doSearch(board, depth - 1 + ext, -beta, -alpha, session, ply + 1, move);
            if (score == VALUE_NONE) {
                board.undoMove();
                return VALUE_NONE;
            }
            score = -score;
        } else {
            int d = depth - 1 - reduction + ext;
            score = doSearch(board, d, -alpha - 1, -alpha, session, ply + 1, move);
            if (score == VALUE_NONE) {
                board.undoMove();
                return VALUE_NONE;
            }
            score = -score;
            if (score > alpha && reduction) {
                session.lmrResearches++;
                score = doSearch(board, depth - 1 + ext, -beta, -alpha, session, ply + 1, move);
                if (score == VALUE_NONE) {
                    board.undoMove();
                    return VALUE_NONE;
                }
                score = -score;
            }
        }

        board.undoMove();
        movesSearched++;

        if (score > maxScore) {
            maxScore = score;
            update_pv(session.pv[ply], move, session.pv[ply + 1]);
        }

        if (score > alpha) {
            alpha = score;

            if (!isCapture) {
                int bonus = depth * depth;
                if (is_win(score))
                    bonus += 4 * depth * depth;
                session.historyHeuristic[(int)move.from()][(int)move.to()] =
                    std::clamp(session.historyHeuristic[(int)move.from()][(int)move.to()] + bonus, -16384, 16384);
            }
        } /*else if (!isCapture && depth > 0) {
            int malus = -depth * depth;
            session.historyHeuristic[(int)move.from()][(int)move.to()] =
                std::clamp(session.historyHeuristic[(int)move.from()][(int)move.to()] + malus, -16384, 16384);
        }*/

        if (alpha >= beta) {
            if (!isCapture) {
                if (session.killerMoves[ply][0] != move) {
                    session.killerMoves[ply][1] = session.killerMoves[ply][0];
                    session.killerMoves[ply][0] = move;
                }
                if (prevMove.is_ok())
                    session.counterMoves[prevMove.from_to()] = move;
            }
            break;
        }

        if (((session.nodes & 2047) == 0 && session.tm.elapsed() >= session.tm.optimum()) ||
            stopSearch.load(std::memory_order_relaxed))
            return VALUE_NONE;
    }
    if (maxScore != -VALUE_INFINITE) {
        TTFlag flag = maxScore >= beta ? LOWERBOUND : maxScore <= alphaOrig ? UPPERBOUND : EXACT;

        tt.store(hash, session.pv[ply][0], value_to_tt(maxScore, ply), depth, flag);
    }
    return maxScore;
}
std::string extract_pv(const chess::Board &root, int maxPly) {
    std::string pv;
    chess::Board pos = root;
    std::unordered_set<uint64_t> visited;
    visited.insert(pos.hash());
    for (int ply = 0; ply < maxPly; ply++) {
        if (pos.is_draw(3))
            break;
        TTEntry *e = search::tt.lookup(pos.hash());
        if (!e)
            break;
        chess::Move m(e->getMove());
        if (!m.is_ok())
            break;
        chess::Movelist ml;
        pos.legals(ml);
        bool legal = false;
        for (size_t i = 0; i < ml.size(); i++)
            if (ml[i] == m) {
                legal = true;
                break;
            }
        if (!legal)
            break;
        pv += chess::uci::moveToUci(m, root.chess960()) + " ";
        if (ply + 1 >= maxPly)
            break;
        pos.doMove(m);
        if (!visited.insert(pos.hash()).second)
            break;
    }
    return pv;
}

void search(const chess::Board &board, const timeman::LimitsType timecontrol) {
    stopSearch = false;
    tt.newSearch();
    static double originalTimeAdjust = -1;
    Session session;
    session.tc = timecontrol;
    session.tm.init(session.tc, board.side_to_move(), board.ply(), originalTimeAdjust);
    session.lastLogTime = session.tm.elapsed();
    session.ogcolor = board.side_to_move();
    chess::Move lastPV[MAX_PLY]{};
    Value prevScore = VALUE_NONE;

    if (!session.tc.searchmoves.empty()) {
        Movelist legal;
        board.legals(legal);
        bool any_legal = false;
        for (size_t i = 0; i < legal.size() && !any_legal; ++i)
            if (std::find(session.tc.searchmoves.begin(),
                          session.tc.searchmoves.end(),
                          chess::uci::moveToUci(legal[i], board.chess960())) != session.tc.searchmoves.end())
                any_legal = true;
        if (!any_legal)
            session.tc.searchmoves.clear();
    }

    {
        Movelist legal;
        board.legals(legal);
        if (legal.size())
            lastPV[0] = legal[0];
    }

    for (int i = 1; i <= timecontrol.depth; i++) {
        session.lastLogTime = session.tm.elapsed();
        session.depth = i;
        for (int _ = 0; _ < 64; _++)
            for (int j = 0; j < 64; j++)
                session.historyHeuristic[_][j] /= 2;
        auto board_ = board;
        Value score_;

        if (i >= 3 && prevScore != VALUE_NONE && !is_win(prevScore) && !is_loss(prevScore)) {
            Value delta = Value(20 + i * 5);
            Value alpha0 = std::max(prevScore - delta, -VALUE_INFINITE);
            Value beta0 = std::min(prevScore + delta, VALUE_INFINITE);

            score_ = doSearch(board_, i, alpha0, beta0, session);

            if (score_ != VALUE_NONE && score_ <= alpha0) {
                score_ = doSearch(board_, i, -VALUE_INFINITE, beta0, session);
                if (score_ != VALUE_NONE && score_ <= alpha0)
                    score_ = doSearch(board_, i, -VALUE_INFINITE, VALUE_INFINITE, session);
            } else if (score_ != VALUE_NONE && score_ >= beta0) {
                score_ = doSearch(board_, i, alpha0, VALUE_INFINITE, session);
                if (score_ != VALUE_NONE && score_ >= beta0)
                    score_ = doSearch(board_, i, -VALUE_INFINITE, VALUE_INFINITE, session);
            }
        } else {
            score_ = doSearch(board_, i, -VALUE_INFINITE, VALUE_INFINITE, session);
        }
        prevScore = score_;
        if (session.tm.elapsed() >= session.tm.optimum() || session.tm.elapsed() >= session.tm.maximum() ||
            stopSearch.load(std::memory_order_relaxed) || score_ == VALUE_NONE)
            break;
        InfoFull info{};
        info.depth = i;
        info.selDepth = session.seldepth;
        info.hashfull = tt.hashfull();
        info.nodes = session.nodes;
        info.nps = session.nodes * 1000 / std::max(session.tm.elapsed(), (timeman::TimePoint)1);
        info.timeMs = session.tm.elapsed();
        info.tbHits = session.tbHits;
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
        info.pv = extract_pv(board, 2 * i + 4);
        // Save first move from PV for bestmove output
        std::string pvStr = info.pv;
        size_t sp = pvStr.find(' ');
        std::string firstMove = (sp == std::string::npos) ? pvStr : pvStr.substr(0, sp);
        if (!firstMove.empty())
            lastPV[0] = chess::Move(chess::uci::uciToMove(board, firstMove).raw());
        std::stringstream ss;
        ss << "qnodes " << session.qnodes << " lmrResearches " << session.lmrResearches << " ttHits " << session.ttHits
           << " ttCutoffs " << session.ttCutoffs << " nullCutoffs " << session.nullCutoffs;
        info.extrainfo = ss.str();
        report(info);
    }
    if (lastPV[0].is_ok())
        report(chess::uci::moveToUci(lastPV[0], board.chess960()));
    else {
        std::cerr << "info string Warning: Did not search\n";
        TTEntry *entry = tt.lookup(board.hash());
        if (entry && entry->getMove() != Move::none().raw())
            report(chess::uci::moveToUci(Move(entry->getMove()), board.chess960()));
        else {
            Movelist moves;
            board.legals(moves);

            if (moves.size()) {
                Board board_ = board;
                Move best = Move::none();
                Value bestScore = -VALUE_INFINITE;
                Session tmpSession{};
                for (Move move : moves) {
                    if (!session.tc.searchmoves.empty() &&
                        std::find(session.tc.searchmoves.begin(),
                                  session.tc.searchmoves.end(),
                                  chess::uci::moveToUci(move, board.chess960())) == session.tc.searchmoves.end())
                        continue;
                    board_.doMove(move);
                    Value score = -qsearch(board_, -VALUE_INFINITE, VALUE_INFINITE, tmpSession, 0);
                    if (score > bestScore) {
                        bestScore = score;
                        best = move;
                    }
                    board_.undoMove();
                }

                if (best.is_ok()) {
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
            } else {
                report("0000");
            }
        }
    }
}
} // namespace engine::search
