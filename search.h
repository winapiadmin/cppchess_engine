#pragma once
#include "eval.h"
<<<<<<< HEAD
int search(chess::Position&, int, int, int, int);
inline int evaluate(chess::Position board){return eval(board);}
void printPV(chess::Position& board);
=======
int search(chess::Board&, int, int, int, int=0);
bool clearTransposition();
>>>>>>> 08822de3005f1e1458cbbb02037d1f714fcd46e7
