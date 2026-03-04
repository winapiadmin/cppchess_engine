#pragma once
#include <fwd_decl.h>
#include <cstdint>
#include <cstdlib>
using Value = int;
namespace engine{

    constexpr int MAX_PLY = 64;
    constexpr Value VALUE_ZERO = 0;
    constexpr Value VALUE_DRAW = 0;
    constexpr Value VALUE_NONE = 32002;
    constexpr Value VALUE_INFINITE = 32001;

    constexpr Value VALUE_MATE = 32000;
    constexpr Value VALUE_MATE_IN_MAX_PLY = VALUE_MATE - MAX_PLY;
    constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY;

    constexpr Value VALUE_TB = VALUE_MATE_IN_MAX_PLY - 1;
    constexpr Value VALUE_TB_WIN_IN_MAX_PLY = VALUE_TB - MAX_PLY;
    constexpr Value VALUE_TB_LOSS_IN_MAX_PLY = -VALUE_TB_WIN_IN_MAX_PLY;
    constexpr bool is_valid(Value value) { return value != VALUE_NONE; }

    constexpr bool is_win(Value value) {
        return value >= VALUE_TB_WIN_IN_MAX_PLY;
    }

    constexpr bool is_loss(Value value) {
        return value <= VALUE_TB_LOSS_IN_MAX_PLY;
    }

    constexpr bool is_decisive(Value value) { return is_win(value) || is_loss(value); }
    constexpr Value MATE(int i) { return VALUE_MATE - i; }
    constexpr Value MATE_DISTANCE(int i) { return VALUE_MATE - (i<0?-i:i); }
    namespace eval {
        Value eval(const chess::Board &board);
        Value piece_value(PieceType pt);
    }
}
