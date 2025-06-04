#pragma once
#include "chess.hpp"
#include "eval.hpp"
namespace movepick
{
    // Static Exchange Evaluation - evaluates material exchange on a square
    int SEE(chess::Board &, chess::Move);
    // Updates killer moves table for move ordering
    void updateKillerMoves(chess::Move, int);
    // Updates history heuristic table for move ordering
    void updateHistoryHeuristic(chess::Move, int);
    // Orders moves for regular search (with TT move, killers at given ply)
    void orderMoves(chess::Board &, chess::Movelist &, chess::Move, chess::Move, int);
    // Orders moves for quiescence search (captures/promotions)
    void qOrderMoves(chess::Board &, chess::Movelist &);
    // Killer moves table: [ply][slot] for move ordering heuristic
    extern chess::Move killerMoves[MAX_PLY][2];
}