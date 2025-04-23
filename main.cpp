#include "chess.hpp"
#include "search.h"
#include <iostream>
<<<<<<< HEAD
extern unsigned long long nodes, tthits;
/*chess::Move find_best_move(chess::Board &board, int depth) {
    int bestScore = -MAX;
    chess::Move bestMove;
    chess::Movelist moves;
    // Use the movegen namespace explicitly to fill the list of legal moves
    chess::movegen::legalmoves(moves, board);

    // Iterate through the Movelist using a range-based for loop
    for (const auto& move : moves) {
        board.makeMove(move);                     // Make the move
        int score = -search(board, depth - 1, -MAX, MAX, 0);
        board.unmakeMove(move);                   // Undo the move

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }

    return bestMove;
}*/

int main(){
	chess::Position board("8/2P5/pk1B4/6b1/7p/8/PP3PB1/1K6 b - - 0 40");
	for (int i=1;i<=254;++i){
		printf("%i ", i);
		auto s=search(board,i,-MAX,MAX,0);
		printf("%i %llu %llu\n", s, nodes, tthits);
		printPV(board);
		nodes=0, tthits=0;
=======
extern unsigned long long nodes;
int main(){
	chess::Board board;
	for (int i=1;i<=8;++i){
	    //Clear transposition
	    if (!clearTransposition())int x=1/0;//force div/0 err
		std::cout<<i<<" ";
		std::cout<<search(board,i,-MAX,MAX,0)<<" "<<nodes<<std::endl;
		nodes=0;
>>>>>>> 08822de3005f1e1458cbbb02037d1f714fcd46e7
	}
	return 0;
}
