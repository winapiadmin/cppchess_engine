#pragma once
#include "eval.h"
#include <fwd_decl.h>
namespace engine::search {
struct Session;
} // namespace engine::search
namespace engine::movepick {
void orderMoves(chess::Board &, chess::Movelist &, chess::Move, int, const engine::search::Session &, chess::Move);
Value see(chess::Board &, chess::Move);
} // namespace engine::movepick
