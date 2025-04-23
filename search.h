#pragma once
#include "eval.h"
int search(chess::Position&, int, int, int, int);
inline int evaluate(chess::Position board){return eval(board);}
void printPV(chess::Position& board);
