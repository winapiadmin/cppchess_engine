#pragma once
#include <fwd_decl.h>
namespace engine::movepick {
void orderMoves(chess::Board &, chess::Movelist &, chess::Move, int);
extern int historyHeuristic[64][64];
extern chess::Move killerMoves[256][2];
} // namespace engine::movepick
