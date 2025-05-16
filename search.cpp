#include "search.hpp"
#include <iostream>

#include "move_ordering.hpp"
namespace search
{
    PV pv;
    // Transposition table
    TranspositionTable tt(20);
    constexpr int NULL_MOVE_REDUCTION = 2,
                  LMR_DEPTH_THRESHOLD = 3,
                  LMR_REDUCTION       = 1;
    std::atomic<bool> stop_requested=false;
    // Main alpha-beta search function
    
    int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth, int ply, PV *pv)
    {
        if (stop_requested)return eval(board);
        nodes++;  // Increment nodes for each evaluated position

        auto current_hash = board.hash();
        auto ttEntry = tt.probe(current_hash);

        if (ttEntry)
        {
            if (ttEntry->depth() >= depth)
            {
                if (ttEntry->flag() == TTFlag::EXACT) {
                    pv->argmove[0] = ttEntry->bestMove;
                    pv->cmove = 1;
                    return ttEntry->score();
                } else if (ttEntry->flag() == TTFlag::LOWERBOUND)
                    alpha = std::max(alpha, ttEntry->score());
                else if (ttEntry->flag() == TTFlag::UPPERBOUND)
                    beta = std::min(beta, ttEntry->score());
                if (alpha >= beta)
                    return ttEntry->score();
            }
        }

        if (depth == 0)
            return quiescence(board, alpha, beta, ply);

        if (depth >= NULL_MOVE_REDUCTION + 1 && !board.inCheck() && beta < MATE(99))
        {
            board.makeNullMove();
            int16_t nullScore = -alphaBeta(board, -beta, -beta + 1, depth - NULL_MOVE_REDUCTION - 1, ply + 1, pv);
            board.unmakeNullMove();
            if (nullScore >= beta)
                return beta;
        }

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
        PV child_pv = {};  // fresh PV for each branch
        chess::Move prevMove = ply?board.move_stack.back() : chess::Move();
        for (size_t i = 0; i < moves.size()&&!stop_requested; ++i) {
            chess::Move move = moves[i];
            child_pv.clear();
            board.makeMove(move);
            bool lateMove = (i >= LMR_DEPTH_THRESHOLD && i > 0);
            int reducedDepth = (lateMove && depth > LMR_REDUCTION) ? (depth - LMR_REDUCTION) : depth;

            uint64_t child_hash = board.hash(); // This is the child's hash, not used for TT storage here
            int16_t score = -alphaBeta(board, -beta, -alpha, reducedDepth - 1, ply + 1, &child_pv);
            board.pop();

            if (score >= beta) {
                if (!board.isCapture(move)) {
                    move_ordering::counterMove[prevMove.from().index()][prevMove.to().index()] = move;
                }
                tt.store(current_hash, move, score, depth, TTFlag::LOWERBOUND);
                return score;
            }

            if (score > alpha) {
                alpha = score;
                bestmove = move;

                pv->argmove[0] = move;
                std::memcpy(&pv->argmove[1], &child_pv.argmove[0], child_pv.cmove * sizeof(chess::Move));
                pv->cmove = 1 + child_pv.cmove;

                foundPV = true;
            }
        }

        tt.store(current_hash, bestmove, alpha, depth, foundPV ? TTFlag::EXACT : TTFlag::UPPERBOUND);
        return alpha;
    }

    int16_t quiescence(chess::Position &board, int16_t alpha, int16_t beta, int ply)
    {
        int16_t standPat = eval(board);
        if (stop_requested)return standPat;
        if (standPat >= beta)
            return beta;
        if (standPat > alpha)
            alpha = standPat;

        chess::Movelist moves;
        chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(moves, board);
        move_ordering::moveOrder(board, moves, ply);

        for (const auto &move : moves)
        {
            if (stop_requested) return alpha;
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

    int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth)
    {
        memset(&pv, 0, sizeof(pv));
        pv.cmove = 0;
        nodes = 0;  // Reset nodes count before starting a new search
        tt.clear_stats();
        return alphaBeta(board, alpha, beta, depth, 0, &pv);
    }

    void printPV(chess::Position &board)
    {
        for (int i = 0; i < pv.cmove && pv.argmove[i] != chess::Move(); i++)
            std::cout << pv.argmove[i] << " ";
        std::cout << std::endl;
    }
} // namespace search
