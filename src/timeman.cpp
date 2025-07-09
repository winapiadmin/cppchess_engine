#include "timeman.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>

namespace timeman {
using namespace std::chrono;

// Global state
TimeControl                         current_tc;  // store the whole TimeControl struct globally
time_point<high_resolution_clock>   start_time;
milliseconds                        inc         = milliseconds(0);
milliseconds                        time_limit  = milliseconds(0);
int                                 moves_to_go = 40;
int                                 depth       = 0;
std::array<int, Stockfish::MAX_PLY> times{};
bool                                infinite = false;

constexpr int    MIN_SAFE_BUFFER_MS  = 100;
constexpr double BUFFER_PERCENTAGE   = 0.05;
constexpr int    MIN_PER_MOVE        = 400;
constexpr int    MAX_PER_MOVE        = 3000;
constexpr double PHASE_SCALE_OPENING = 0.7;
constexpr double PHASE_SCALE_MIDDLE  = 1.0;
constexpr double PHASE_SCALE_ENDGAME = 1.25;
constexpr int    OPENING_DEPTH       = 5;
constexpr int    MIDDLE_DEPTH        = 10;

static double get_phase_scale(int current_depth) {
    if (current_depth <= OPENING_DEPTH)
        return PHASE_SCALE_OPENING;
    else if (current_depth <= MIDDLE_DEPTH)
        return PHASE_SCALE_MIDDLE;
    else
        return PHASE_SCALE_ENDGAME;
}

inline void reset_start_time() { start_time = high_resolution_clock::now(); }
void        setLimits(const TimeControl& tc) {
    current_tc  = tc;
    infinite    = tc.infinite;
    moves_to_go = std::max(1, tc.movestogo);
    depth       = tc.depth;

    if (infinite && tc.depth > 0)
    {
        time_limit = milliseconds::max();
        inc        = milliseconds(0);
        std::cout << "[TimeMan] Infinite or fixed depth mode.\n";
    }
    else if (tc.movetime >= 0 && tc.movetime < INFINITE_TIME)
    {
        // UCI: movetime overrides all other time settings
        time_limit = milliseconds(tc.movetime);
        inc        = milliseconds(0);
        std::cout << "[TimeMan] Using fixed movetime: " << tc.movetime << " ms\n";
    }
    else
    {
        // Fall back to clock-based time control
        int time_left = tc.white_to_move ? tc.wtime : tc.btime;
        int inc_ms    = tc.white_to_move ? tc.winc : tc.binc;

        if (time_left != INFINITE_TIME)
        {
            int base_time = time_left / moves_to_go;
            int safe_time = base_time / 16 + inc_ms;
            safe_time     = std::clamp(safe_time, MIN_PER_MOVE, MAX_PER_MOVE);

            double scale = get_phase_scale(depth);
            safe_time    = static_cast<int>(safe_time * scale);

            time_limit = milliseconds(safe_time);
            inc        = milliseconds(inc_ms);

            std::cout << "[TimeMan] Using clock. Left: " << time_left << " ms, inc: " << inc_ms
                      << " ms, moves_to_go: " << moves_to_go << ", scaled: " << safe_time
                      << " ms\n";
        }
    }

    reset_start_time();
}

bool check_time() {
    if (infinite)
        return false;

    int elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start_time).count();

    int dynamic_buffer =
      std::max(MIN_SAFE_BUFFER_MS, static_cast<int>(time_limit.count() * BUFFER_PERCENTAGE));

    bool out_of_time = elapsed + dynamic_buffer >= time_limit.count();

    if (out_of_time)
    {
        std::cout << "[TimeMan] Out of time! Elapsed: " << elapsed
                  << " ms, limit: " << time_limit.count() << " ms, buffer: " << dynamic_buffer
                  << " ms" << std::endl;
    }

    return out_of_time;
}
}  // namespace timeman
