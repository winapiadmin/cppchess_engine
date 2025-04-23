#include "search.h"
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <vector>
#include <iostream>
unsigned long long nodes = 0, tthits=0;
std::vector<chess::Move> principalVariation;

int search(chess::Position &board, int depth, int alpha, int beta, int ply) {
    if (board.inCheck() && board.isGameOver().first != chess::GameResultReason::NONE) return MATE(ply);
    if (board.isGameOver().first != chess::GameResultReason::NONE) return 0;
    if (depth == 0) {
        return eval(board);
    }
	if (ply==0){
		principalVariation.clear();
		principalVariation.resize(depth);
	}
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    chess::Move best_move;
    bool is_pv_node = true;

    for (const auto &move : moves) {
        board.makeMove(move);
        nodes++;
        int score;
        if (is_pv_node) {
            score = -search(board, depth - 1, -beta, -alpha, ply + 1);
        } else {
            score = -search(board, depth - 1, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && score < beta) {
            	nodes++;
                score = -search(board, depth - 1, -beta, -alpha, ply + 1);
            }
        }
        board.unmakeMove(move);

        if (score >= beta) {
            return beta;  // Beta cutoff
        }
        if (score > alpha) {
            alpha = score;
            best_move = move;
            principalVariation[ply]=move;
        }
        is_pv_node = false;
    }

    return alpha;
}
void printPV(chess::Position &board) {
    for (const auto &move : principalVariation) {
        std::cout << move << " ";
    }
    std::cout <<'\n';
}
