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
      chess::Move argmove[64]; // The line.
      void clear() {
        cmove = 0;
        // Optional, but useful:
        for (auto &m : argmove)
            m = chess::Move();
      }
    };
    inline uint64_t nodes=0;  // Track number of nodes
    inline std::chrono::milliseconds time_limit;  // Global time limit
    inline std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    extern std::atomic<bool> stop_requested;
    int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth);
    int16_t quiescence(chess::Position &board, int16_t alpha, int16_t beta, int ply);
    bool check_time();
    extern TranspositionTable tt;
    extern PV pv;
}
