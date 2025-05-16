// search.hpp

#pragma once
#include <chrono>
#include "eval.h"
#include "tt.hpp"
#include <atomic>
namespace search
{
    struct PV
    {
      int cmove; // Number of moves in the line.
      chess::Move argmove[256]; // The line.
      void clear() {
        cmove = 0;
        // Optional, but useful:
        for (auto &m : argmove)
            m = chess::Move();
      }
    };
    inline uint64_t nodes=0;  // Track number of nodes
    extern std::chrono::milliseconds time_limit;  // Global time limit

    extern std::atomic<bool> stop_requested;
    int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth);
    int16_t quiescence(chess::Position &board, int16_t alpha, int16_t beta, int ply);
    void printPV(chess::Position &board);
    extern TranspositionTable tt;
    extern PV pv;
}
