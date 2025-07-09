#pragma once
#include "types.h"
#include <chrono>
constexpr int INFINITE_TIME = 86400000;
struct TimeControl {
    int     wtime         = INFINITE_TIME;
    int     btime         = INFINITE_TIME;
    int     winc          = 0;
    int     binc          = 0;
    int     movestogo     = 40;
    int     movetime      = INFINITE_TIME;
    uint8_t depth         = Stockfish::MAX_PLY;
    bool    infinite      = false;
    bool    white_to_move = true;
};

namespace timeman {
extern std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
extern std::chrono::milliseconds                                   time_buffer;
extern std::chrono::milliseconds                                   inc;
extern std::chrono::milliseconds                                   time_limit;
extern int                                                         moves_to_go;
extern bool                                                        infinite;

bool check_time();

// Set time control limits using the TimeControl struct
void setLimits(const TimeControl& tc);
}  // namespace timeman
