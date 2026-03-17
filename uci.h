#pragma once
#include "score.h"
#include <string>
#include <string_view>
namespace engine {
struct InfoShort {
  int depth;
  Score score;
};

struct InfoFull : InfoShort {
  int selDepth;
  size_t multiPV;
  std::string_view wdl;
  std::string_view bound;
  size_t timeMs;
  size_t nodes;
  size_t nps;
  size_t tbHits;
  std::string_view pv;
  int hashfull;
};

struct InfoIteration {
  int depth;
  std::string_view currmove;
  size_t currmovenumber;
};
std::string format_score(const Score &s);
void report(const InfoFull &info, bool showWDL = false);
void report(const InfoShort &info);
void report(const InfoIteration &info);
void report(std::string_view bestmove);
void loop();
class OptionsMap;
extern OptionsMap options;
} // namespace engine