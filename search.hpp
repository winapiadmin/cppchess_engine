// File: search.hpp
#pragma once

#include "eval.h"
#include "tt.hpp"
#include "move_ordering.hpp"
namespace search {
    
    inline uint64_t nodes = 0;

    int16_t alphaBeta(chess::Position& board, int16_t alpha, int16_t beta, int depth, int ply);
    int16_t quiescence(chess::Position& board, int16_t alpha, int16_t beta, int ply);
    void printPV(chess::Position& board);
    extern TranspositionTable tt;
} // namespace search
