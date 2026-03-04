#pragma once
#include <fwd_decl.h>
namespace engine::movepick {
	void orderMoves(chess::Board &, chess::Movelist &, chess::Move, int);
	extern Value historyHeuristic[64][64]{};
    extern Move killerMoves[256][2];
}
