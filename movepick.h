#pragma once
#include <fwd_decl.h>
namespace engine::movepick {
	void orderMoves(chess::Board &, chess::Movelist &, chess::Move, int);
}