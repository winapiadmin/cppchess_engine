#include "search.h"
#include "evaluate.h"
#include <memory>
#include <vector>
#include <chrono>
#include <iostream>
#include <atomic>

namespace search {
using namespace Stockfish;  // maybe....
using Move = uint16_t;

std::unique_ptr<Stockfish::Eval::NNUE::Networks>          nn;
std::unique_ptr<Stockfish::Eval::NNUE::AccumulatorCaches> cache;
TranspositionTable                                        tt;
std::chrono::time_point<std::chrono::steady_clock> start;
std::atomic<bool> stop_requested;
int               seldepth = 0;

struct SearchStackEntry {
    std::unique_ptr<Move[]> pv;
    Value                   eval = Stockfish::VALUE_NONE;
    
    SearchStackEntry() :
        pv(std::make_unique<Move[]>(Stockfish::MAX_PLY)) {}
};

inline Value value_draw(size_t nodes) { return VALUE_DRAW - 1 + Value(nodes & 0x2); }
inline Value value_to_tt(Value v, int ply) {
    return is_win(v) ? v + ply : is_loss(v) ? v - ply : v;
}

static Value value_from_tt(Value v, int ply, int r50c) {
    if (is_win(v))
    {
        if (v >= VALUE_MATE_IN_MAX_PLY && VALUE_MATE - v > 100 - r50c)
            return VALUE_TB_WIN_IN_MAX_PLY - 1;
        if (VALUE_TB - v > 100 - r50c)
            return VALUE_TB_WIN_IN_MAX_PLY - 1;
        return v - ply;
    }
    if (is_loss(v))
    {
        if (v <= VALUE_MATED_IN_MAX_PLY && VALUE_MATE + v > 100 - r50c)
            return VALUE_TB_LOSS_IN_MAX_PLY + 1;
        if (VALUE_TB + v > 100 - r50c)
            return VALUE_TB_LOSS_IN_MAX_PLY + 1;
        return v + ply;
    }
    return v;
}

static void update_pv(Move* pv, Move move, const Move* childPv) {
    *pv++ = move;
    while (childPv && *childPv)
        *pv++ = *childPv++;
    *pv = 0;
}
static Value qsearch(Position& pos, Value alpha, Value beta, int ply, SearchStackEntry* ss) {
    seldepth = std::max(seldepth, ply+1);
	
    Value stand_pat = -VALUE_INFINITE;
    if (!pos.b.inCheck())
    {
        stand_pat = Stockfish::Eval::evaluate(*nn, pos, pos.stack, *cache, 0);
        ss->eval  = stand_pat;
        if (stand_pat >= beta)
            return stand_pat;
        if (stand_pat > alpha)
            alpha = stand_pat;
    }

    chess::Movelist list, list2;
    chess::movegen::legalmoves(list, pos.b);
    if (list.size() == 0)
        return pos.b.inCheck() ? mated_in(ply + 1) : value_draw(nodes.load());
    for (int i = 0; i < list.size(); ++i)
    {
        chess::Move mv = list[i];
        if (pos.b.isCapture(mv) || pos.b.givesCheck(mv) != chess::CheckType::NO_CHECK
            || mv.typeOf() == mv.PROMOTION)
            list2.add(mv);
    }
    movepick::qOrderMoves(pos.b, list2);

    for (int i = 0; i < list2.size(); ++i)
    {
        chess::Move mv = list2[i];
        pos.do_move(mv);
        ++nodes;
        Value score = -qsearch(pos, -beta, -alpha, ply + 1, ss + 1);
        pos.undo_move(mv);

        if (score >= beta)
            return score;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

static Value
negamax(Position& pos, int depth, Value alpha, Value beta, int ply, SearchStackEntry* ss) {
    seldepth = std::max(seldepth, ply+1);
	if (stop_requested) return VALUE_NONE;
    if (pos.b.isRepetition(1) || pos.b.halfMoveClock() >= 99)
        return VALUE_DRAW;

    uint64_t key = pos.b.hash();
    ss->pv[0]    = 0;

    TTEntry* entry = tt.lookup(key);
    if (entry && entry->depth >= depth)
    {
        Value tt_score = value_from_tt(entry->score, ply, pos.b.halfMoveClock());
        if ((entry->flag == TTFlag::EXACT)
            || (entry->flag == TTFlag::LOWERBOUND && tt_score >= beta)
            || (entry->flag == TTFlag::UPPERBOUND && tt_score <= alpha))
            return tt_score;
    }

    if (depth <= 0)
        return qsearch(pos, alpha, beta, ply, ss);

    chess::Movelist list;
    chess::movegen::legalmoves(list, pos.b);
    if (list.empty())
        return pos.b.inCheck() ? mated_in(ply + 1) : value_draw(nodes.load());

    chess::Move bestMove;
    Value       best = -VALUE_INFINITE, orig_alpha = alpha;

    movepick::orderMoves(pos.b, list, entry ? entry->move : chess::Move::NULL_MOVE, ply);

    for (int i = 0; i < list.size(); ++i)
    {
        chess::Move mv = list[i];
        if (ply == 0){
			auto  end   = std::chrono::steady_clock::now();
			auto  s = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
			if (s>=1)
            std::cout << "info currmove " << chess::uci::moveToUci(mv) << " currmovenum " << i + 1
                      << std::endl;
		}
        pos.do_move(mv);
        ++nodes;
        Value score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, ss + 1);
        pos.undo_move(mv);
        if (stop_requested) break;
        if (score > best)
        {
            best     = score;
            bestMove = mv;
            update_pv(ss->pv.get(), mv.move(), (ss + 1)->pv.get());
        }

        if (score > alpha)
            alpha = score;
        if (score >= beta)
        {
            if (!pos.b.isCapture(mv) && pos.b.givesCheck(mv) == chess::CheckType::NO_CHECK
                && mv.typeOf() != mv.PROMOTION)
            {
                movepick::updateHistoryHeuristic(mv, depth);
                movepick::updateKillerMoves(mv, ply);
            }
            break;
        }
    }
	if (!stop_requested){
		TTFlag flag = (best >= beta)       ? TTFlag::LOWERBOUND
					: (best <= orig_alpha) ? TTFlag::UPPERBOUND
										   : TTFlag::EXACT;

		tt.store(key, bestMove, value_to_tt(best, ply), depth, flag);
	}
    ss->eval = best;
    return best;
}
void run_search(const chess::Board& board, const TimeControl& tc) {
	std::vector<SearchStackEntry> ss(MAX_PLY);
    int                           rundepth = tc.depth ? tc.depth : 5;
    Position pos(board);
    std::vector<Move> pv(MAX_PLY);
    seldepth                               = 0;
    start = std::chrono::steady_clock::now();
    for (int d = 1; d <= rundepth && !stop_requested; ++d)
    {
        cache->clear(*nn);
        for (auto& s : ss)
        {
            std::fill(s.pv.get(), s.pv.get() + MAX_PLY, 0);
            s.eval = VALUE_NONE;
        }

        pos.stack.reset();
        Value v     = negamax(pos, d, -VALUE_INFINITE, VALUE_INFINITE, 0, ss.data());
        auto  end   = std::chrono::steady_clock::now();
        auto  nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        std::cout << "info depth " << d << " seldepth " << seldepth << " score ";
        if (is_decisive(v))
        {
            int m = (v > 0 ? VALUE_MATE - v : -VALUE_MATE - v);
            std::cout << "mate " << (v > 0 ? (m + 1) / 2 : -(m + 1) / 2);
        }
        else
        {
			std::cout << "cp " << v;
        }
        std::cout << " nodes " << nodes << " nps " << (nodes * 1000000000 / nanos);
        std::cout << " time " << nanos / 1000000 << " hashfull " << tt.hashfull() << " pv ";
        for (int j = 0; j < MAX_PLY && ss[0].pv[j]; ++j)
            std::cout << chess::uci::moveToUci(chess::Move(ss[0].pv[j])) << " ";
        std::cout << std::endl;
		if (!stop_requested) {
			for (int i = 0; i < MAX_PLY; ++i) {
				pv[i] = ss[0].pv[i];
			}
		}

    }

    if (pv[0])
        std::cout << "bestmove " << chess::uci::moveToUci(chess::Move(pv[0])) << std::endl;
}

// ---------------------- Initialization ---------------------------
template<bool init_nn>
void init() {
    if constexpr (init_nn)
    {
        Stockfish::Eval::NNUE::NetworkBig nBig(
          Stockfish::Eval::NNUE::EvalFile{EvalFileDefaultNameBig, "", EvalFileDefaultNameBig},
          Stockfish::Eval::NNUE::EmbeddedNNUEType::BIG);
        Stockfish::Eval::NNUE::NetworkSmall nSmall(
          Stockfish::Eval::NNUE::EvalFile{EvalFileDefaultNameSmall, "", EvalFileDefaultNameSmall},
          Stockfish::Eval::NNUE::EmbeddedNNUEType::SMALL);
        nn = std::make_unique<Stockfish::Eval::NNUE::Networks>(std::move(nBig), std::move(nSmall));
    }
    else
    {
        nn->big.verify(EvalFileDefaultNameBig, onVerify);
        nn->small.verify(EvalFileDefaultNameSmall, onVerify);
        cache = std::make_unique<Stockfish::Eval::NNUE::AccumulatorCaches>(*nn);
    }
}

// Explicit instantiations for the linker
template void init<true>();
template void init<false>();

}  // namespace search
