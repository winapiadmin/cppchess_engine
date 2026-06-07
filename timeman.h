#pragma once
#include <chrono>
#include <types.h>
#include <vector>
namespace engine::timeman {
using TimePoint = std::chrono::milliseconds::rep;
constexpr TimePoint INFINITE_TIME = 864000000;
// LimitsType struct stores information sent by the caller about the analysis
// required.
inline TimePoint now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
struct LimitsType {

  // Init explicitly due to broken value-initialization of non POD in MSVC
  LimitsType() {
    time[chess::WHITE] = time[chess::BLACK] = inc[chess::WHITE] =
        inc[chess::BLACK] = movetime = TimePoint(0);
    movestogo = mate = perft = infinite = 0;
    depth = 64;
    nodes = 0;
    ponderMode = false;
  }

  bool use_time_management() const {
    return time[chess::WHITE] || time[chess::BLACK];
  }

  std::vector<std::string> searchmoves;
  TimePoint time[chess::COLOR_NB], inc[chess::COLOR_NB], movetime, startTime;
  int movestogo, depth, mate, perft, infinite;
  uint64_t nodes;
  bool ponderMode;
};
class TimeManagement {
public:
  void init(LimitsType &limits, chess::Color us, int ply,
            double &originalTimeAdjust);

  TimePoint optimum() const;
  TimePoint maximum() const;
  TimePoint elapsed() const { return elapsed_time(); }
  TimePoint elapsed_time() const { return now() - startTime; };

  void clear();

private:
  TimePoint startTime;
  TimePoint optimumTime;
  TimePoint maximumTime;
};
} // namespace engine::timeman