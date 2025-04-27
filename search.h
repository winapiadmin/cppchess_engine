#pragma once
#include "eval.h"
int search(chess::Position& RESTRICT board, const int, int, const int, const int);
void printPV(const chess::Position& RESTRICTboard);
#define MAX_DEPTH 100