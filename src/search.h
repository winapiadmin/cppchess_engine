#ifndef CHESS_ENGINE_SEARCH_H
#define CHESS_ENGINE_SEARCH_H
#include "evaluate.h"
#include "movepick.hpp"
#include "position.h"
#include "timeman.hpp"
#include "tt.hpp"
#include "types.h"
#include "nnue/network.h"
#include <atomic>
namespace search {
inline std::atomic<uint64_t> nodes{0};
struct SearchParams {
    TimeControl tc;
};
template<bool init_nn>
void init();  // Called twice (one for UCIOptions NNUE paths and one for anything else
void run_search(const chess::Board& board, const TimeControl& tc);
extern std::atomic<bool>                                stop_requested;
extern TranspositionTable                               tt;
extern std::unique_ptr<Stockfish::Eval::NNUE::Networks> nn;  // defer construction
inline void onVerify(std::string_view sv) { std::cout << sv << std::endl; }
}  // namespace search
#endif  // CHESS_ENGINE_SEARCH_H
