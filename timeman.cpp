#include "timeman.h"
#include "uci.h"
#include "ucioption.h"
#include <iostream>
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <cmath>
namespace engine::timeman
{

    TimePoint TimeManagement::optimum() const { return optimumTime; }
    TimePoint TimeManagement::maximum() const { return maximumTime; }

    void TimeManagement::clear() {
    }

    // Called at the beginning of the search and calculates
    // the bounds of time allowed for the current game ply. We currently support:
    //      1) x basetime (+ z increment)
    //      2) x moves in y seconds (+ z increment)
    void TimeManagement::init(LimitsType &limits,
        chess::Color        us,
        int                 ply,
        double &originalTimeAdjust) {

        // If we have no time, we don't need to fully initialize TM.
        // startTime is used by movetime and useNodesTime is used in elapsed calls.
        startTime = limits.startTime;

        if (limits.time[us] == 0)
            return;

        // optScale is a percentage of available time to use for the current move.
        // maxScale is a multiplier applied to optimumTime.
        double optScale, maxScale;

        // These numbers are used where multiplications, divisions or comparisons
        // with constants are involved.
        const TimePoint time = limits.time[us];
		const int moveOverhead = options["Move Overhead"];
        // Maximum move horizon
        int centiMTG = limits.movestogo ? std::min(limits.movestogo * 100, 5000) : 5051;

        // If less than one second, gradually reduce mtg
        if (time < 1000)
            centiMTG = int(time * 5.051);

        // Make sure timeLeft is > 0 since we may use it as a divisor
        TimePoint timeLeft =
            std::max(TimePoint(1),
                limits.time[us]
                + (limits.inc[us] * (centiMTG - 100) - moveOverhead * (200 + centiMTG)) / 100);

        // x basetime (+ z increment)
        // If there is a healthy increment, timeLeft can exceed the actual available
        // game time for the current move, so also cap to a percentage of available game time.
        if (limits.movestogo == 0)
        {
            // Extra time according to timeLeft
            if (originalTimeAdjust < 0)
                originalTimeAdjust = 0.3128 * std::log10(timeLeft) - 0.4354;

            // Calculate time constants based on current time left.
            double logTimeInSec = std::log10(time / 1000.0);
            double optConstant = std::min(0.0032116 + 0.000321123 * logTimeInSec, 0.00508017);
            double maxConstant = std::max(3.3977 + 3.03950 * logTimeInSec, 2.94761);

            optScale = std::min(0.0121431 + std::pow(ply + 2.94693, 0.461073) * optConstant,
                0.213035 * limits.time[us] / timeLeft)
                * originalTimeAdjust;

            maxScale = std::min(6.67704, maxConstant + ply / 11.9847);
        }

        // x moves in y seconds (+ z increment)
        else
        {
            optScale =
                std::min((0.88 + ply / 116.4) / (centiMTG / 100.0), 0.88 * limits.time[us] / timeLeft);
            maxScale = 1.3 + 0.11 * (centiMTG / 100.0);
        }

        // Limit the maximum possible time for this move
        optimumTime = TimePoint(optScale * timeLeft);
        maximumTime =
            TimePoint(std::min(0.825179 * limits.time[us] - moveOverhead, maxScale * optimumTime)) - 10;
    }

}
