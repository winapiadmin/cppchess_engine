#pragma once
#include "eval.h"
#include "timeman.h"
#include "tt.h"
#include <position.h>
namespace engine::search {
struct Session {
    timeman::TimeManagement tm;
    timeman::LimitsType tc;
    int seldepth = 0;
    uint64_t nodes = 0, qnodes = 0, lmrResearches = 0;
    uint64_t tbHits = 0, ttHits = 0, ttCutoffs = 0, nullCutoffs = 0;
    chess::Move pv[MAX_PLY][MAX_PLY];
    Value historyHeuristic[chess::SQUARE_NB][chess::SQUARE_NB]{};
    chess::Move killerMoves[MAX_PLY][2];
    chess::Move counterMoves[4096];
    timeman::TimePoint lastLogTime;
    int depth = 0;
    chess::Color ogcolor;
};
void stop();
void search(const chess::Board &, const timeman::LimitsType);
bool isStopped();
extern engine::TranspositionTable tt;
} // namespace engine::search
