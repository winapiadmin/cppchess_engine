#include "search.hpp"
namespace search
{
    TranspositionTable tt(20); // 16MB
    chess::Move PV[128];
    int pvLength = 0;
    const auto INVALID_MOVE = chess::Move();
    void clearPV()
    {
        std::memset(PV, 0, sizeof(PV));
        pvLength = 0;
    }

    void printPV(chess::Position &board)
    {
        std::cout << "PV: ";
        for (int i = 0; i < pvLength && PV[i] != INVALID_MOVE; ++i)
            std::cout << PV[i] << " ";
        std::cout << std::endl;
    }

    int16_t quiescence(chess::Position &board, int16_t alpha, int16_t beta, int ply)
    {
        nodes++;
        int16_t standPat = eval(board);

        if (standPat >= beta)
            return beta;
        if (standPat > alpha)
            alpha = standPat;

        chess::Movelist moves;
        chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(moves, board);
        move_ordering::scoreMoves(board, moves, ply);

        for (auto &move : moves)
        {
            board.makeMove(move);
            int16_t score = -quiescence(board, -beta, -alpha, ply + 1);
            board.unmakeMove(move);

            if (score >= beta)
                return beta;
            if (score > alpha)
                alpha = score;
        }

        return alpha;
    }
    void updatePV(int ply, chess::Move move)
    {

        // Clear the rest to avoid garbage from previous searches
        for (int j = ply; j < 128; ++j)
            PV[j] = INVALID_MOVE;
        PV[ply] = move;
        // Count PV length from root
        pvLength = 0;
        while (pvLength < 128 && PV[pvLength] != INVALID_MOVE)
            ++pvLength;
    }
int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth, int ply)
{
    ++nodes;

    auto hash = board.hash();
    if (TTEntry *entry = tt.probe(hash))
    {
        if (entry->depth() >= depth)
        {
            if (entry->flag() == 0)
            {
                // No PV update here (exact TT hit)
                return entry->score();
            }
            else if (entry->flag() == 1 && entry->score() >= beta)
            {
                // Beta cutoff
                return entry->score();
            }
            else if (entry->flag() == 2 && entry->score() <= alpha)
            {
                // Alpha cutoff
                return entry->score();
            }
        }
    }

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    if (moves.empty())
    {
        if (board.inCheck())
        {
            return MATE(ply);
        }
        else
        {
            return 0;
        }
    }

    if (ply == 0)
    {
        clearPV();
    }

    if (depth == 0)
    {
        return eval(board);  // Evaluate the leaf node
    }

    move_ordering::scoreMoves(board, moves, ply);
    chess::Move bestMove;
    int16_t bestScore = -MATE(0);

    // Search with first move (narrow window)  PVS (Principal Variation Search)
    auto firstMove = moves.begin();
    board.makeMove(*firstMove);
    int16_t score = -alphaBeta(board, -beta, -alpha + 1, depth - 1, ply + 1);
    board.pop();
    
    if (score > bestScore)
    {
        bestScore = score;
        bestMove = *firstMove;

        // Update PV for this best move
        updatePV(ply, bestMove);
    }

    // Now search the remaining moves (full window)
    for (auto &move : moves)
    {
        if (move == bestMove)
            continue;  // Skip the best move (already searched)

        board.makeMove(move);
        score = -alphaBeta(board, -alpha - 1, -alpha, depth - 1, ply + 1);
        board.pop();

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = move;

            // Update PV for this best move
            updatePV(ply, bestMove);
        }

        if (score >= beta)
        {
            tt.store(hash, move, beta, depth, 1); // Store as lowerbound
            move_ordering::updateKillers(move, ply);
            return beta;
        }
    }

    // Store best move in TT
    uint8_t flag = (bestScore <= alpha) ? 2 : (bestScore >= beta) ? 1 : 0;
    tt.store(board.hash(), bestMove, bestScore, depth, flag);

    // After the full search, update pvLength based on the current PV
    if (ply == 0)
    {
        pvLength = 0;
        while (pvLength < 128 && PV[pvLength] != INVALID_MOVE)
            ++pvLength;
    }

    return bestScore;
}

}