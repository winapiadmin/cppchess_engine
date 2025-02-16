#pragma once
#include "pieces.h"

// **Evaluation Function Declarations**
int doubled(U64, U64);
int backward(U64, U64);
int weaks(chess::Board&);
int blockage(chess::Board&);
int pawnIslands(chess::Board&);
int isolated(chess::Board&);
int dblisolated(chess::Board&);
int holes(chess::Board&);
int pawnRace(chess::Board&);
int underpromote(chess::Board&);
int weakness(chess::Board&);
int pawnShield(chess::Board&);
int pawnStorm(chess::Board&);
int pawnLevers(chess::Board&);
int outpost(chess::Board&);
int evaluatePawnRams(chess::Board&);
int evaluateUnfreePawns(chess::Board&);
int evaluateOpenPawns(chess::Board&);
