#pragma once
#include <position.h>
#include "tt.h"
namespace engine {
    namespace timeman {
        struct LimitsType;
    }
}
namespace engine::search {
    void stop();
    void search(const chess::Board&, const timeman::LimitsType);
    extern engine::TranspositionTable tt;
}