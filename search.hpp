#pragma once
#include "chess.hpp"
#include "timeman.hpp"
#include "tt.hpp"
#include "movepick.hpp"
#include "eval.hpp"
#include "ucioptions.hpp"
#include <inttypes.h>
#include <atomic>
#include <cstdint>
namespace search
{
    inline std::atomic<uint64_t> nodes{0};
    void run_search(const chess::Board& board, const TimeControl& tc);
    extern std::atomic<bool> stop_requested;
    extern TranspositionTable tt;
}
