#pragma once
#include "tt.h"
#include <position.h>
namespace engine {
namespace timeman {
struct LimitsType;
}
} // namespace engine
namespace engine::search {
void stop();
void search(const chess::Board &, const timeman::LimitsType);
extern engine::TranspositionTable tt;
} // namespace engine::search