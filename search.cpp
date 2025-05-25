#include "search.hpp"
#include <iostream>
#include "move_ordering.hpp"

namespace search
{
    constexpr int NULL_MOVE_DEPTH_THRESHOLD = 3;
    constexpr int LMR_MOVE_COUNT_THRESHOLD = 4;
    constexpr int LMR_AGGRESSIVE_THRESHOLD = 6;
    constexpr int LMR_NORMAL_REDUCTION = 1;
    constexpr int LMR_AGGRESSIVE_REDUCTION = 2;
    constexpr int NULL_MOVE_REDUCTION = 2;
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
        nodes++;

        if (ply >= MAX_PLY)
            return eval(board);

        int16_t stand_pat = eval(board);
        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;

        chess::Movelist moves;
        chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(moves, board);

        if (moves.size() == 0)
            return stand_pat;

        move_ordering::moveOrder(board, moves, ply);

        const int16_t futility_margin = 975;
        const int16_t promotion_bonus = 775;

        for (const auto &move : moves)
        {
            // Delta pruning with pre-calculated margins
            if (stand_pat + futility_margin + (move.typeOf() == chess::Move::PROMOTION ? promotion_bonus : 0) < alpha)
                continue;

            board.makeMove(move);
            int16_t score = -quiescence(board, -beta, -alpha, ply + 1);
            board.pop();

            if (score >= beta)
                return beta;

            alpha = std::max(alpha, score);
        }
        return alpha;
    }
    int detectExtension(chess::Position& board, chess::Move move, bool givesCheck) {
        int ext = 0;
        
        // Check extension
        if (givesCheck) ext++;
        
        // Passed pawn extension
        if (passed(board) >= 0) ext++;
        
        // Pawn to 7th rank extension
        if (move.typeOf() == chess::Move::NORMAL && 
            board.at(move.from()).type() == chess::PieceType::PAWN && 
            ((int)move.to().rank() == 6 || (int)move.to().rank() == 1)) {
            ext++;
        }
        return ext;
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
                pv->argmove[pv->cmove = 1] = entry->bestMove;
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

        if (depth == 0)
            return quiescence(board, alpha, beta, ply);
        int16_t static_eval = eval(board);
        const int16_t futility_margin = 150;
        // Null move pruning
        if (depth >= 3 && !board.inCheck() && board.hasNonPawnMaterial(board.sideToMove()))
        {
            board.makeNullMove();
            int16_t null_score = -alphaBeta(board, -beta, -beta + 1, depth - 1 - 2, ply + 1, pv);
            board.unmakeNullMove();

            if (null_score >= beta)
                return beta;
        }

        chess::Movelist moves;
        chess::movegen::legalmoves(moves, board);

        if (moves.empty())
            return board.inCheck() ? -MATE(ply) : 0;

        move_ordering::moveOrder(board, moves, ply); // Assume TT, history heuristic included

        int16_t best_score = -MATE(ply);
        chess::Move best_move;
        int moveCount = 0;
        bool is_pv = true, in_check = !board.inCheck();

        for (const auto &move : moves)
        {
            bool is_capture = board.isCapture(move);
            bool is_promo = move.typeOf() & chess::Move::PROMOTION;
            bool gives_check = board.givesCheck(move) != chess::CheckType::NO_CHECK;

            if (stop_requested.load())
                break;
            PV child_pv;
            child_pv.clear();
            if (depth == 1 && !in_check && !is_capture && !is_promo && static_eval + futility_margin <= alpha)
                continue; // Futile move, unlikely to raise alpha

            // Late Move Reductions
            int reduction = 0;
            if (depth >= 3 && moveCount >= LMR_MOVE_COUNT_THRESHOLD && !in_check && !is_capture && !gives_check)
            {
                reduction = (moveCount >= LMR_AGGRESSIVE_THRESHOLD) ? LMR_AGGRESSIVE_REDUCTION : LMR_NORMAL_REDUCTION;
                if (depth - 1 - reduction < 1)
                    reduction = 0;
            }
            const int extension=0;//detectExtension(board, move, gives_check);
            board.makeMove(move);
            int16_t score;

            if (is_pv)
            {
                // Full window for first move
                score = -alphaBeta(board, -beta, -alpha, depth - 1, ply + 1, &child_pv);
            }
            else
            {
                // PVS with LMR
                if (reduction)
                {
                    score = -alphaBeta(board, -alpha - 1, -alpha, depth - 1 - reduction+extension, ply + 1, &child_pv);
                    if (score > alpha)
                        score = -alphaBeta(board, -alpha - 1, -alpha, depth - 1 +extension, ply + 1, &child_pv);
                    if (score > alpha && score < beta)
                        score = -alphaBeta(board, -beta, -alpha, depth - 1+extension, ply + 1, &child_pv);
                }
                else
                {
                    score = -alphaBeta(board, -alpha - 1, -alpha, depth - 1+extension, ply + 1, &child_pv);
                    if (score > alpha && score < beta)
                        score = -alphaBeta(board, -beta, -alpha, depth - 1+extension, ply + 1, &child_pv);
                }
            }

            board.pop();
            moveCount++;

            if (score > best_score)
            {
                best_score = score;
                best_move = move;

                // Update PV
                pv->argmove[0] = move;
                std::memcpy(pv->argmove + 1, child_pv.argmove, sizeof(chess::Move) * child_pv.cmove);
                pv->cmove = child_pv.cmove + 1;

                if (score > alpha)
                {
                    alpha = score;
                    is_pv = false;
                    if (score >= beta)
                        break; // Beta cutoff
                }
            }
        }

        // Store result in transposition table
        TTFlag flag;
        if (best_score <= original_alpha)
            flag = TTFlag::UPPERBOUND;
        else if (best_score >= beta)
            flag = TTFlag::LOWERBOUND;
        else
            flag = TTFlag::EXACT;

        tt.store(hash, best_move, best_score, depth, flag);

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
