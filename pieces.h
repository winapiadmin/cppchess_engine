#pragma once
#include "t.h"
#include "pawns.h"

// **1. Game Phase Evaluation**
int phase(const chess::Board board);
// **2. Piece Activity Evaluations**
int evaluateBadBishops(const chess::Board board);
int evaluateFianchetto(const chess::Board board);
int evaluateTrappedPieces(const chess::Board board);
int evaluateKnightForks(const chess::Board board);
int evaluateRooksOnFiles(const chess::Board board);

// **3. King Safety & Mobility Evaluations**
int evaluateKingSafety(const chess::Board board);
int evaluateKingPawnTropism(const chess::Board board);
int evaluateKingMobility(const chess::Board board);

// **4. Positional & Space Evaluations**
int evaluateSpaceControl(const chess::Board board);
int evaluateTempo(const chess::Board board);
