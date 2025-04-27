#include "chess.hpp"
#include "search.h"
#include <iostream>
extern unsigned long long nodes, evalhits, tthits;
int searchRoot(chess::Position& board, int depth, int prevScore) {
    if (depth < 3) {
        return search(board, depth, -MAX, MAX, 0);
    }

    int window = 16;
    int alpha = prevScore - window;
    int beta = prevScore + window;
    int score;

    while (true) {
        score = search(board, depth, alpha, beta, 0);
        if (score <= alpha) {
            alpha -= window;
        } else if (score >= beta) {
            beta += window;
        } else {
            break;
        }
        window *= 2;
    }

    return score;
}
int main() {
    chess::Position board("8/2P5/pk1B4/6b1/7p/8/PP3PB1/1K6 b - - 0 40");
    int prevScore = 0;

    for (int i = 1; i <= 10; ++i) {
        clock_t t1 = clock();

        int score = search(board, i, -MAX, MAX, 0);//searchRoot(board, i, prevScore);

        clock_t t2 = clock();
        double seconds = double(t2 - t1) / CLOCKS_PER_SEC;
        printf("\n%i %i %llu %llu %llu %.2f NPS\n", i, score, nodes, evalhits, tthits, nodes/seconds);
        t1=clock();
        printPV(board);
        t2=clock();
        printf("\nPV time: %.2f seconds\n", double(t2 - t1) / CLOCKS_PER_SEC);
        prevScore = score;
        nodes = 0, evalhits=0, tthits=0;
    }

    return 0;
}
