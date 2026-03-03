#include "score.h"
#include <cassert>
namespace engine {
    Score::Score(Value v) {
        assert(-VALUE_INFINITE < v && v < VALUE_INFINITE);

        if (!is_decisive(v))
        {
            score = InternalUnits{ v };
        }
        else if (std::abs(v) <= VALUE_TB)
        {
            auto distance = VALUE_TB - std::abs(v);
            score = (v > 0) ? Tablebase{ distance, true } : Tablebase{ -distance, false };
        }
        else
        {
            auto distance = VALUE_MATE - std::abs(v);
            score = (v > 0) ? Mate{ distance } : Mate{ -distance };
        }
    }
}