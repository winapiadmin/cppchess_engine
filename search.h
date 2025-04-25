#pragma once
#include "eval.h"
int search(chess::Position&, int, int, int, int);
void printPV(const chess::Position& board);
#define MAX_DEPTH 100