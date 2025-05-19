#include "search.hpp"
#include <iostream>
#include "move_ordering.hpp"

namespace search
{
    PV pv;
    TranspositionTable tt(20);
    std::atomic<bool> stop_requested = false;
    auto time_buffer = std::chrono::milliseconds(50); // larger buffer to stop early
    bool check_time()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        return (elapsed + time_buffer >= time_limit);
    }

    int16_t quiescence(chess::Position &board, int16_t alpha, int16_t beta, int ply)
    {
        int16_t stand_pat = eval(board);
        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;

        chess::Movelist moves;
        chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(moves, board);
        move_ordering::moveOrder(board, moves, ply);
        int16_t best_score = -MATE(ply);
        for (const auto &move : moves)
        {
            board.makeMove(move);
            int16_t score = -quiescence(board, -beta, -alpha, ply + 1);
            board.pop();

            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    alpha = score;
                    if (score >= beta)
                        break;
                }
            }
        }
        return best_score;
    }
    int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth, int ply, PV *pv)
    {
        if (stop_requested.load())
            return eval(board);

        if (check_time())
        {
            stop_requested.store(true);
            return eval(board);
        }

        nodes++;
        pv->clear();
        int16_t original_alpha = alpha;

        auto hash = board.hash();
        TTEntry *entry = tt.probe(hash);

        if (entry && entry->depth() >= depth)
        {
            int16_t score = entry->score();
            if (entry->flag() == TTFlag::EXACT)
            {
                std::memcpy(pv->argmove, entry->pv, sizeof(chess::Move) * entry->pvLength());
                pv->cmove = entry->pvLength();
                return score;
            }
            else if (entry->flag() == TTFlag::LOWERBOUND)
                alpha = std::max(alpha, score);
            else if (entry->flag() == TTFlag::UPPERBOUND)
                beta = std::min(beta, score);
            if (alpha >= beta)
                return score;
        }

        if (board.isRepetition(1))
            return 0;

        chess::Movelist moves;
        chess::movegen::legalmoves(moves, board);

        if (moves.empty())
            return board.inCheck() ? -MATE(ply) : 0;

        if (depth == 0)
            return quiescence(board, alpha, beta, ply);

        move_ordering::moveOrder(board, moves, ply);

        int16_t best_score = -MATE(ply);
        chess::Move best_move;
        int moveCount = 0;

        for (const auto &move : moves)
        {
            // Early check for stop request before making move
            if (stop_requested.load())
            {
                break;
            }

            PV child_pv;
            child_pv.clear();

            board.makeMove(move);

            int reduction = 0;
            if (depth >= 3 && moveCount >= 4 && !board.inCheck() && !board.isCapture(move))
            {
                reduction = (moveCount >= 6) ? 2 : 1;
            }

            int16_t score;
            if (reduction)
            {
                score = -alphaBeta(board, -beta, -alpha, depth - 1 - reduction, ply + 1, &child_pv);
                if (score > alpha)
                    score = -alphaBeta(board, -beta, -alpha, depth - 1, ply + 1, &child_pv);
            }
            else
            {
                score = -alphaBeta(board, -beta, -alpha, depth - 1, ply + 1, &child_pv);
            }

            board.pop();
            moveCount++;

            if (score > best_score)
            {
                best_score = score;
                best_move = move;

                // Store best move into PV, even if score <= alpha
                pv->argmove[0] = move;
                std::memcpy(pv->argmove + 1, child_pv.argmove, sizeof(chess::Move) * child_pv.cmove);
                pv->cmove = child_pv.cmove + 1;

                if (score > alpha)
                {
                    alpha = score;
                    if (score >= beta)
                        break; // Beta cutoff
                }
            }
        }

        // Save TT with the best move and PV
        TTFlag flag;
        if (best_score <= original_alpha)
            flag = TTFlag::UPPERBOUND;
        else if (best_score >= beta)
            flag = TTFlag::LOWERBOUND;
        else
            flag = TTFlag::EXACT;

        tt.store(hash, best_move, best_score, depth, flag, pv->argmove, pv->cmove);
        return best_score;
    }

    int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth)
    {
        nodes = 0;
        stop_requested = false;
        start_time = std::chrono::high_resolution_clock::now();
        tt.clear_stats();

        PV local_pv;
        local_pv.clear();
        int16_t score = alphaBeta(board, alpha, beta, depth, 0, &local_pv);

        std::memcpy(pv.argmove, local_pv.argmove, sizeof(chess::Move) * local_pv.cmove);
        pv.cmove = local_pv.cmove;
        return score;
    }
}
