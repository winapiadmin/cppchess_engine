#pragma once
#include "eval.h"
#include <cstddef>
#include <position.h>
#include <string>

namespace engine::tb {

void init(const std::string &path);
constexpr int TB_ERROR = 1000;
std::size_t wdl_count();
std::size_t dtz_count();
int probe_wdl(chess::Board &board);
int probe_dtz(chess::Board &board);
} // namespace engine::tb
