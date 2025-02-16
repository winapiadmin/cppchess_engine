#pragma once
#include "t.h"
#include "pawns.h"

// **1. Game Phase Evaluation**
int phase(const chess::Board&);
// **2. Piece Activity Evaluations**
int evaluateBadBishops(const chess::Board&);
int evaluateFianchetto(const chess::Board&);
int evaluateTrappedPieces(const chess::Board&);
int evaluateKnightForks(const chess::Board&);
int evaluateRooksOnFiles(const chess::Board&);

// **3. King Safety & Mobility Evaluations**
int evaluateKingSafety(const chess::Board&);
int evaluateKingPawnTropism(const chess::Board&);
int evaluateKingMobility(const chess::Board&);

// **4. Positional & Space Evaluations**
int evaluateSpaceControl(const chess::Board&);
int evaluateTempo(const chess::Board&);
