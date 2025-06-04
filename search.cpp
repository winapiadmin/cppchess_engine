#include "search.hpp"
#include "timeman.hpp"
#include <cstring>
#include <chrono>
#include <cstdio>
#include <cinttypes>
#include <algorithm>

namespace search
{
    struct PV
    {
        int8_t cmove;
        chess::Move pv[MAX_PLY];
    };

    constexpr int LMR_BONUS = 2;
    std::atomic<bool> stop_requested(false);
    PV stablePV;
    TranspositionTable tt;
    int seldepth = 0;

    struct SearchAbortException : public std::exception
    {
        SearchAbortException() = default;
    };

    inline bool isMateScore(int16_t score)
    {
        return std::abs(score) > MAX_SCORE_CP;
    }

    int16_t adjustMateScore(int16_t score, int ply)
    {
        return isMateScore(score) ? (score > 0 ? score - ply : score + ply) : score;
    }

    int16_t restoreMateScore(int16_t score, int ply)
    {
        return isMateScore(score) ? (score > 0 ? score + ply : score - ply) : score;
    }

    int16_t quiescence(chess::Board &board, int16_t alpha, int16_t beta, int ply)
    {
        seldepth = std::max(seldepth, ply);
        if (timeman::check_time() || stop_requested.load(std::memory_order_relaxed))
            throw SearchAbortException();

        ++nodes;

        int16_t stand_pat = eval(board);
        if (stand_pat >= beta)
            return beta;
        if (alpha < stand_pat)
            alpha = stand_pat;

        chess::Movelist moves;
        chess::movegen::legalmoves(moves, board);
        if (moves.empty())
            return board.inCheck() ? -MATE(ply) : 0;

        chess::Movelist tacticalMoves;
        for (auto mv : moves)
        {
            if (board.isCapture(mv) || (mv.typeOf() & chess::Move::PROMOTION) || board.givesCheck(mv) != chess::CheckType::NO_CHECK)
                tacticalMoves.add(mv);
        }

        movepick::qOrderMoves(board, tacticalMoves);
        for (auto mv : tacticalMoves)
        {
            // --- BEGIN SEE FILTER ---
            int gain = mv.score(); // Or use precomputed SEE
            if (board.isCapture(mv) && gain < 0)
                continue;
            // --- END SEE FILTER ---

            // --- BEGIN DELTA PRUNING ---
            int margin = 50;
            if (stand_pat + gain + margin < alpha)
                continue;
            // --- END DELTA PRUNING ---

            board.makeMove(mv);
            int16_t score = -quiescence(board, -beta, -alpha, ply + 1);
            board.unmakeMove(mv);

            if (score >= beta)
                return beta;
            if (score > alpha)
                alpha = score;
        }
        return alpha;
    }

    int16_t alpha_beta(chess::Board &board, int depth, int16_t alpha, int16_t beta, int ply, PV *pv, int ext = 0)
    {
        pv->cmove = 0;
        if (timeman::check_time() || stop_requested.load(std::memory_order_relaxed))
            throw SearchAbortException();

        uint64_t h = board.hash();
        TTEntry *entry = tt.lookup(h);
        chess::Move hash_move = entry ? entry->bestMove : chess::Move::NULL_MOVE;

        if (entry && entry->hash == h && entry->depth() >= depth)
        {
            int16_t score = adjustMateScore(entry->score(), ply);
            if (entry->flag() == TTFlag::LOWERBOUND && score >= beta)
                return score;
            if (entry->flag() == TTFlag::UPPERBOUND && score <= alpha)
                return score;
            if (entry->flag() == TTFlag::EXACT)
                return score;
        }

        chess::Movelist moves;
        chess::movegen::legalmoves(moves, board);
        if (moves.empty())
            return board.inCheck() ? -MATE(ply) : 0;

        if (depth <= 0){
            ++nodes;
            return quiescence(board, alpha, beta, ply);
        }

        bool in_check = board.inCheck();
        int16_t evalScore = eval(board);

        // --- BEGIN RAZORING ---
        if (depth <= 2 && !in_check)
        {
            if (evalScore + 150 < alpha)
                return quiescence(board, alpha, beta, ply);
        }
        // --- END RAZORING ---

        bool can_do_null = (!in_check) && (depth >= 3);
        if (can_do_null)
        {
            board.makeNullMove();
            int R = depth / 6;
            int16_t score = -alpha_beta(board, depth - 1 - R, -beta, -beta + 1, ply + 1, pv);
            board.unmakeNullMove();
            if (score >= beta)
            {
                tt.store(h, chess::Move::NULL_MOVE, restoreMateScore(score, ply), depth, TTFlag::LOWERBOUND);
                return score;
            }
        }

        bool has_hash = (hash_move != chess::Move::NULL_MOVE);
        if (!has_hash && depth >= 4)
        {
            PV temp;
            temp.cmove = 0;
            alpha_beta(board, depth - 2, alpha, beta, ply, &temp);
            TTEntry *iid = tt.lookup(board.hash());
            if (iid && iid->bestMove != chess::Move::NULL_MOVE)
                hash_move = iid->bestMove;
        }

        movepick::orderMoves(board, moves, hash_move, stablePV.pv[0], ply);

        chess::Move best_move = chess::Move();
        int16_t best_score = -MATE(0);

        for (int i = 0; i < moves.size(); ++i)
        {
            chess::Move mv = moves[i];
            PV sub_pv;
            sub_pv.cmove = 0;

            bool isCap = board.isCapture(mv);
            bool do_lmr = (i > 0) && !isCap && !in_check;
            bool givesCheck = board.givesCheck(mv) != chess::CheckType::NO_CHECK;
            bool isPromo = mv.typeOf() & chess::Move::PROMOTION;

            // --- BEGIN FUTILITY PRUNING ---
            if (depth <= 3 && !in_check && !isCap && !isPromo && !givesCheck)
            {
                const int margin = 100 * depth;
                if (evalScore + margin <= alpha)
                    continue;
            }
            // --- END FUTILITY PRUNING ---

            // --- BEGIN EXTENSIONS ---
            int extension = 0;
            if (givesCheck)
                extension = 1;

            if (board.at<chess::PieceType>(mv.from()) == chess::PieceType::PAWN && (mv.to().rank() >= chess::Rank::RANK_6))
            {
                int rank = board.sideToMove() == chess::Color::WHITE ? (int)mv.to().rank() : 7 - mv.to().rank();
                if (rank >= 5)
                    extension += 1;
            }

            if (moves.size() == 1)
                extension += 1;

            if (ext + extension > 16)
                extension = 0;

            int new_depth = depth - 1 + extension;
            // --- END EXTENSIONS ---

            board.makeMove(mv);
            int16_t score;

            if (do_lmr && !extension && depth >= 3)
            {
                score = -alpha_beta(board, new_depth - LMR_BONUS, -alpha - 1, -alpha, ply + 1, &sub_pv, ext);
                if (score > alpha)
                    score = -alpha_beta(board, new_depth, -beta, -alpha, ply + 1, &sub_pv, ext);
            }
            else
            {
                if (i == 0)
                    score = -alpha_beta(board, new_depth, -beta, -alpha, ply + 1, &sub_pv, ext + extension);
                else
                {
                    score = -alpha_beta(board, new_depth, -alpha - 1, -alpha, ply + 1, &sub_pv, ext + extension);
                    if (score > alpha)
                        score = -alpha_beta(board, new_depth, -beta, -alpha, ply + 1, &sub_pv, ext + extension);
                }
            }

            board.unmakeMove(mv);

            if (score > best_score)
            {
                best_score = score;
                best_move = mv;
                if (score > alpha)
                {
                    alpha = score;
                    pv->pv[0] = mv;
                    std::memcpy(&pv->pv[1], sub_pv.pv, sizeof(chess::Move) * sub_pv.cmove);
                    pv->cmove = sub_pv.cmove + 1;
                }
            }

            if (score >= beta)
            {
                if (!isCap)
                {
                    movepick::updateKillerMoves(mv, ply);
                    movepick::updateHistoryHeuristic(mv, depth);
                }
                break;
            }
        }

        if (best_score != -MATE(0))
        {
            TTFlag flag = (best_score >= beta)   ? TTFlag::LOWERBOUND
                          : (best_score > alpha) ? TTFlag::EXACT
                                                 : TTFlag::UPPERBOUND;
            tt.store(h, best_move, restoreMateScore(best_score, ply), depth, flag);
        }

        return best_score;
    }

    void run_search(const chess::Board &board, const TimeControl &tc)
    {
        nodes = 0, seldepth = 0;
        stop_requested.store(false, std::memory_order_relaxed);
        timeman::setLimits(tc);

        PV child_pv;
        child_pv.cmove = 0;
        stablePV.cmove = 0;

        chess::Movelist rootMoves;
        chess::movegen::legalmoves(rootMoves, board);

        int16_t last_score = 0;
        int max_iter = std::min(tc.infinite ? MAX_PLY : tc.depth, (int)MAX_PLY);

        for (int current_depth = 1; current_depth <= max_iter; ++current_depth)
        {
            chess::Board _board = board;
            child_pv.cmove = 0;
            int16_t alpha = last_score - 30;
            int16_t beta = last_score + 30;

            try
            {
                last_score = alpha_beta(_board, current_depth, alpha, beta, 0, &child_pv);

                if (last_score <= alpha || last_score >= beta)
                {
                    alpha = -MATE(0);
                    beta = MATE(0);
                    last_score = alpha_beta(_board, current_depth, alpha, beta, 0, &child_pv);
                }

                stablePV = child_pv;
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::high_resolution_clock::now() - timeman::start_time)
                                   .count();

                if (isMateScore(last_score)){
					// Confusement: break
                    printf("info depth %d seldepth %d nodes %" PRIu64 " nps %" PRIu64 " time %u score mate %d pv ",
                           current_depth,
                           seldepth,
                           nodes.load(),
                           (elapsed == 0 ? 0 : nodes.load() / elapsed * 1000),
                           elapsed,
                           last_score > 0 ? (MAX_MATE - last_score) : -(MAX_MATE + last_score));
				}
                else
                    printf("info depth %d seldepth %d nodes %" PRIu64 " nps %" PRIu64 " time %u score cp %d pv ",
                           current_depth,
                           seldepth,
                           nodes.load(),
                           (elapsed == 0 ? 0 : nodes.load() / elapsed * 1000),
                           elapsed,
                           last_score);

                for (int j = 0; j < child_pv.cmove; ++j)
                    printf("%s ", chess::uci::moveToUci(child_pv.pv[j], board.chess960()).c_str());
                printf("\n");
				
				// [DEBUG] 
				if (isMateScore(last_score)) break;
                if (timeman::check_time())
                    break;
            }
            catch (SearchAbortException &)
            {
                break;
            }
        }

        if (stablePV.cmove > 0)
        {
            printf("bestmove %s\n",
                   chess::uci::moveToUci(stablePV.pv[0], board.chess960()).c_str());
        }
        else if (!rootMoves.empty())
        {
            printf("bestmove %s\n",
                   chess::uci::moveToUci(rootMoves[0], board.chess960()).c_str());
        }
        else
        {
            printf("bestmove 0000\n");
        }
    }
}
