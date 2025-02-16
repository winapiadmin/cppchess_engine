#ifndef PAWNS_H
#define PAWNS_H

#include "pieces.h"  // Ensure correct includes

int isolated(const chess::Board& board);
int dblisolated(const chess::Board& board);
int weaks(const chess::Board& board);
int blockage(const chess::Board& board);
int holes(const chess::Board& board);
int underpromote(chess::Board &board);
int weakness(const chess::Board& board);
int evaluatePawnRams(const chess::Board& board);
int pawnIslands(const chess::Board& board);
int pawnRace(const chess::Board& board);
int pawnShield(const chess::Board& board);
int pawnStorm(const chess::Board& board);
int pawnLevers(const chess::Board& board);
int outpost(const chess::Board& board);
int evaluateUnfreePawns(const chess::Board& board);
int evaluateOpenPawns(const chess::Board& board);

#endif  // PAWNS_H
