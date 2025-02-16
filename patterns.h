#pragma once
#include "chess.hpp"
#include "t.h"

bool isLosingQueenPromotion(int, U64, U64, U64);
inline bool isLosingQueenPromotion(U64 $1, U64 $2, U64 $3, U64 $4){ return isLosingQueenPromotion(__builtin_ctzll($1), $2, $3, $4);}
int evaluateTactics(const chess::Board &board);
