#pragma once
#include "chess.hpp"
#include "patterns.h"
#include "pieces.h"
#include <map>
#include <vector>
#define MAX 32767 // for black
#define MAX_MATE 32000
#define Pawn 100
#define Knight 300
#define Bishop 300
#define Rook 500
#define Queen 900
#define MATE(i) MAX-i
#define MATE_DISTANCE(i) i-MAX_MATE
int eval(chess::Board&);
