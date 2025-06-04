#pragma once
#include "chess.hpp"
#include <cstdint>
#include <cstdlib>

constexpr int MAX_PLY      = 64;
constexpr int MAX_MATE     = 32'000;
constexpr int MAX_SCORE_CP = 31'000;
inline int16_t MATE(int i) { return MAX_MATE - i; }
inline int16_t MATE_DISTANCE(int16_t i) { return MAX_MATE - abs(i); }
int16_t eval(const chess::Board &board);
void trace(const chess::Board &board);
int16_t piece_value(chess::PieceType);
// some clarity for documentation
#define Evaluation [[nodiscard]]
#define AllPhases Evaluation
#define Middlegame Evaluation
#define Endgame Evaluation
