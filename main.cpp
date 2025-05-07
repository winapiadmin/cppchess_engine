// File: main.cpp
#include "chess.hpp"
#include "eval.h"
#include "search.hpp"
#include "tt.hpp"
#include <iostream>
#include <ctime>
int main()
{
    chess::Position board;
    int prevScore = 0;

    for (int depth = 1; depth <= 6; ++depth)
    {
        clock_t t1 = clock();

        search::tt.newSearch(); // Reset timestamps per iteration
        int score = search::alphaBeta(board, -MATE(0), MATE(0), depth, 0);

        clock_t t2 = clock();
        double seconds = double(t2 - t1) / CLOCKS_PER_SEC;

        printf("\nDepth: %d | Score: %d | Nodes: %llu | TT Hits: %llu | TT Misses: %llu | NPS: %.2f\n",
               depth, score, search::nodes, tthits, ttmiss, search::nodes / seconds);

        search::printPV(board);

        prevScore = score;
        search::nodes = 0;
        search::tt.clear_stats();
    }

    return 0;
}
