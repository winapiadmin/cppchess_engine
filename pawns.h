#pragma once
#include "pieces.h"

// **Evaluation Function Declarations**
int doubled(U64, U64);
int backward(U64, U64);
int weaks(const chess::Board&);
int blockage(const chess::Board&);
int pawnIslands(const chess::Board&);
int isolated(const chess::Board&);
int dblisolated(const chess::Board&);
int holes(const chess::Board&);
int pawnRace(const chess::Board&);
int underpromote(chess::Board&);
int weakness(const chess::Board&);
int pawnShield(const chess::Board&);
int pawnStorm(const chess::Board&);
int pawnLevers(const chess::Board&);
int outpost(const chess::Board&);
int evaluatePawnRams(const chess::Board&);
int evaluateUnfreePawns(const chess::Board&);
int evaluateOpenPawns(const chess::Board&);
