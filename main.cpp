#include "chess.hpp"
#include "search.h"
#include <iostream>
extern unsigned long long nodes;
int main(){
	chess::Board board;
	for (int i=1;i<=8;++i){
	    //Clear transposition
	    if (!clearTransposition())int x=1/0;//force div/0 err
		std::cout<<i<<" ";
		std::cout<<search(board,i,-MAX,MAX,0)<<" "<<nodes<<std::endl;
		nodes=0;
	}
	return 0;
}
