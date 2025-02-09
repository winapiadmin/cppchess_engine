#include "chess.hpp"
#include "search.h"
#include <iostream>
extern unsigned long long nodes;
int main(){
	chess::Board board;
	for (int i=2;i<245;++i){
		std::cout<<i<<" ";
		std::cout<<search(board,i,-MAX,MAX)<<" "<<nodes<<std::endl;
		nodes=0;
	}
	return 0;
}
