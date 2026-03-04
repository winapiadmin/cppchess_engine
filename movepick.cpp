#include "movepick.h"
#include "eval.h"
#include <position.h>
using namespace chess;
namespace engine {
    Value historyHeuristic[SQUARE_NB][SQUARE_NB]{};
    Move killerMoves[MAX_PLY][2];
    void movepick::orderMoves(chess::Board & board, chess::Movelist & moves, chess::Move ttMove, int ply)
    {
        std::vector<std::pair<chess::Move, Value>> scoredMoves;
        scoredMoves.reserve(moves.size());
    
        for (const auto& move : moves)
        {
            Value score = 0;
    
            if (move == ttMove)
                score = 10000;
            else if (board.isCapture(move))
                score = 
                    ((move.type() & EN_PASSANT)==0?piece_value(board.at<PieceType>(move.to())):piece_value(PAWN))*10 - piece_value(board.at<PieceType>(move.from()));
            else if (move == killerMoves[ply][0])
                score = 8500;
            else if (move == killerMoves[ply][1])
                score = 8000;
            else
                score = historyHeuristic[move.from().index()][move.to().index()];
    
            scoredMoves.emplace_back(move, score);
        }
    
        std::stable_sort(scoredMoves.begin(), scoredMoves.end(),
            [](const auto& a, const auto& b)
            {
                return a.second > b.second;
            });
    
        for (size_t i = 0; i < scoredMoves.size(); ++i)
            moves[i] = scoredMoves[i].first;
    }
}
