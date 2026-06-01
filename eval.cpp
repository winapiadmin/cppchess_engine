#include "eval.h"
#include "tune.h"
#include <iostream>
#include <position.h>
using namespace chess;
using namespace engine::eval;

namespace engine::eval {
Value PawnValue = 100, KnightValue = 325, BishopValue = 350, RookValue = 500,
      QueenValue = 900, KingValue = 0;
Value mg_pawn_table[64] = {
    0,   0,  0,   0,   0,   0,  0,  0,   98,  134, 61, 95,  68, 126, 34, -11,
    -6,  7,  26,  31,  65,  56, 25, -20, -14, 13,  6,  21,  23, 12,  17, -23,
    -27, -2, -5,  12,  17,  6,  10, -25, -26, -4,  -4, -10, 3,  3,   33, -12,
    -35, -1, -20, -23, -15, 24, 38, -22, 0,   0,   0,  0,   0,  0,   0,  0,
};

Value eg_pawn_table[64] = {
    0,  0,   0,  0,  0,  0,  0,  0,  178, 173, 158, 134, 147, 132, 165, 187,
    94, 100, 85, 67, 56, 53, 82, 84, 32,  24,  13,  5,   -2,  4,   17,  17,
    13, 9,   -3, -7, -7, -8, 3,  -1, 4,   7,   -6,  1,   0,   -5,  -1,  -8,
    13, 8,   8,  10, 13, 0,  2,  -7, 0,   0,   0,   0,   0,   0,   0,   0,
};

Value mg_knight_table[64] = {
    -167, -89, -34, -49, 61,   -97, -15, -107, -73, -41, 72,  36,  23,
    62,   7,   -17, -47, 60,   37,  65,  84,   129, 73,  44,  -9,  17,
    19,   53,  37,  69,  18,   22,  -13, 4,    16,  13,  28,  19,  21,
    -8,   -23, -9,  12,  10,   19,  17,  25,   -16, -29, -53, -12, -3,
    -1,   18,  -14, -19, -105, -21, -58, -33,  -17, -28, -19, -23,
};

Value eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99, -25, -8,  -25, -2,  -9,
    -25, -24, -52, -24, -20, 10,  9,   -1,  -9,  -19, -41, -17, 3,
    22,  22,  22,  11,  8,   -18, -18, -6,  16,  25,  16,  17,  4,
    -18, -23, -3,  -1,  15,  10,  -3,  -20, -22, -42, -20, -10, -5,
    -2,  -20, -23, -44, -29, -51, -23, -15, -22, -18, -50, -64,
};

Value mg_bishop_table[64] = {
    -29, 4,  -82, -37, -25, -42, 7,  -8, -26, 16, -18, -13, 30,  59,  18,  -47,
    -16, 37, 43,  40,  35,  50,  37, -2, -4,  5,  19,  50,  37,  37,  7,   -2,
    -6,  13, 13,  26,  34,  12,  10, 4,  0,   15, 15,  15,  14,  27,  18,  10,
    4,   15, 16,  0,   7,   21,  33, 1,  -33, -3, -14, -21, -13, -12, -39, -21,
};

Value eg_bishop_table[64] = {
    -14, -21, -11, -8, -7, -9, -17, -24, -8,  -4, 7,   -12, -3, -13, -4, -14,
    2,   -8,  0,   -1, -2, 6,  0,   4,   -3,  9,  12,  9,   14, 10,  3,  2,
    -6,  3,   13,  19, 7,  10, -3,  -9,  -12, -3, 8,   10,  13, 3,   -7, -15,
    -14, -18, -7,  -1, 4,  -9, -15, -27, -23, -9, -23, -5,  -9, -16, -5, -17,
};

Value mg_rook_table[64] = {
    32,  42,  32,  51, 63, 9,  31, 43,  27,  32,  58,  62,  80, 67, 26,  44,
    -5,  19,  26,  36, 17, 45, 61, 16,  -24, -11, 7,   26,  24, 35, -8,  -20,
    -36, -26, -12, -1, 9,  -7, 6,  -23, -45, -25, -16, -17, 3,  0,  -5,  -33,
    -44, -16, -20, -9, -1, 11, -6, -71, -19, -13, 1,   17,  16, 7,  -37, -26,
};

Value eg_rook_table[64] = {
    13, 10, 18, 15, 12, 12, 8,   5,   11, 13, 13, 11, -3, 3,   8,  3,
    7,  7,  7,  5,  4,  -3, -5,  -3,  4,  3,  13, 1,  2,  1,   -1, 2,
    3,  5,  8,  4,  -5, -6, -8,  -11, -4, 0,  -5, -1, -7, -12, -8, -16,
    -6, -6, 0,  2,  -9, -9, -11, -3,  -9, 2,  3,  -1, -5, -13, 4,  -20,
};

Value mg_queen_table[64] = {
    -28, 0,   29, 12,  59, 44, 43, 45, -24, -39, -5,  1,   -16, 57,  28,  54,
    -13, -17, 7,  8,   29, 56, 47, 57, -27, -27, -16, -16, -1,  17,  -2,  1,
    -9,  -26, -9, -10, -2, -4, 3,  -3, -14, 2,   -11, -2,  -5,  2,   14,  5,
    -35, -8,  11, 2,   8,  15, -3, 1,  -1,  -18, -9,  10,  -15, -25, -31, -50,
};

Value eg_queen_table[64] = {
    -9,  22,  22,  27,  27,  19,  10,  20,  -17, 20,  32,  41,  58,
    25,  30,  0,   -20, 6,   9,   49,  47,  35,  19,  9,   3,   22,
    24,  45,  57,  40,  57,  36,  -18, 28,  19,  47,  31,  34,  39,
    23,  -16, -27, 15,  6,   9,   17,  10,  5,   -22, -23, -30, -16,
    -16, -23, -36, -32, -33, -28, -22, -43, -5,  -32, -20, -41,
};

Value mg_king_table[64] = {
    -65, 23,  16,  -15, -56, -34, 2,   13,  29,  -1,  -20, -7,  -8,
    -4,  -38, -29, -9,  24,  2,   -16, -20, 6,   22,  -22, -17, -20,
    -12, -27, -30, -25, -14, -36, -49, -1,  -27, -39, -46, -44, -33,
    -51, -14, -14, -22, -46, -44, -30, -15, -27, 1,   7,   -8,  -64,
    -43, -16, 9,   8,   -15, 36,  12,  -54, 8,   -28, 24,  14,
};

Value eg_king_table[64] = {-74, -35, -18, -18, -11, 15,  4,   -17, -12, 17, 14,
                           17,  17,  38,  23,  11,  10,  17,  23,  15,  20, 45,
                           44,  13,  -8,  22,  24,  27,  26,  33,  26,  3,  -18,
                           -4,  21,  24,  27,  23,  9,   -11, -19, -3,  11, 21,
                           23,  16,  7,   -9,  -27, -11, 4,   13,  14,  4,  -5,
                           -17, -53, -34, -21, -11, -28, -14, -24, -43};

Value *mg_pesto_table[] = {nullptr,         mg_pawn_table, mg_knight_table,
                           mg_bishop_table, mg_rook_table, mg_queen_table,
                           mg_king_table};

Value *eg_pesto_table[] = {nullptr,         eg_pawn_table, eg_knight_table,
                           eg_bishop_table, eg_rook_table, eg_queen_table,
                           eg_king_table};
Value mgMobility[] = {
    0, // none
    0, // pawn
    3, // knight
    5, // bishop
    2, // rook
    1, // queen
    0  // king
};

Value egMobility[] = {0, 0, 2, 5, 3, 2, 0};
Value spaceWeight = 28;
// tuning slop here
Value eval(const chess::Board &board) {
  constexpr int KnightPhase = 1;
  constexpr int BishopPhase = 1;
  constexpr int RookPhase = 2;
  constexpr int QueenPhase = 4;
  constexpr int TotalPhase =
      KnightPhase * 4 + BishopPhase * 4 + RookPhase * 4 + QueenPhase * 2;
  const int sign = board.side_to_move() == WHITE ? 1 : -1;
  int mgScore = 0;
  int egScore = 0;
  int phase = 0;
  {
    mgScore = egScore = board.side_to_move() == WHITE ? spaceWeight : 0;
    Bitboard occ = board.occ(), occ2 = occ;
    while (occ) {
      Square sq = (Square)pop_lsb(occ), _sq = sq;
      auto p = board.at(sq);
      int _sign = 1;
      if (color_of(p) == BLACK) {
        _sign = -1;
        _sq = square_mirror(sq);
      }
      auto pt = piece_of(p);
      if (pt == NO_PIECE_TYPE)
        continue;
      mgScore += _sign * mg_pesto_table[pt][_sq];
      egScore += _sign * eg_pesto_table[pt][_sq];
      mgScore += _sign * piece_value(pt);
      egScore += _sign * piece_value(pt);
      switch (pt) {
      case KNIGHT:
        phase += KnightPhase;
        break;
      case BISHOP:
        phase += BishopPhase;
        break;
      case ROOK:
        phase += RookPhase;
        break;
      case QUEEN:
        phase += QueenPhase;
        break;
      case PAWN:
        break;
      case KING:
        break;
      default:
        break;
      }
      Bitboard attacks = 0;

      switch (pt) {
      case KNIGHT:
        attacks = chess::attacks::knight(sq);
        break;

      case BISHOP:
        attacks = chess::attacks::bishop(sq, occ2);
        break;

      case ROOK:
        attacks = chess::attacks::rook(sq, occ2);
        break;

      case QUEEN:
        attacks = chess::attacks::queen(sq, occ2);
        break;

      default:
        break;
      }
      attacks &= ~board.us(color_of(p));
      int mobility = popcount(attacks);
      mgScore += _sign * mobility * mgMobility[pt];
      egScore += _sign * mobility * egMobility[pt];
    }
  }
  // Bishop pair bonus
  for (Color c : {WHITE, BLACK}) {
    if (board.count(BISHOP, c) >= 2) {
      int s = (c == WHITE) ? 1 : -1;
      mgScore += s * 30;
      egScore += s * 10;
    }
  }

  // Rook on open/semi-open file
  for (Color c : {WHITE, BLACK}) {
    int s = (c == WHITE) ? 1 : -1;
    Bitboard rooks = board.pieces(ROOK, c);
    while (rooks) {
      Square sq = Square(pop_lsb(rooks));
      File f = file_of(sq);
      Bitboard fileMask = attacks::MASK_FILE[f];
      bool hasOwnPawn = (board.pieces(PAWN, c) & fileMask) != 0;
      bool hasEnemyPawn = (board.pieces(PAWN, ~c) & fileMask) != 0;
      if (!hasOwnPawn && !hasEnemyPawn) {
        mgScore += s * 25;
        egScore += s * 10;
      } else if (!hasOwnPawn) {
        mgScore += s * 15;
        egScore += s * 5;
      }
    }
  }

  // Doubled pawn penalty
  for (Color c : {WHITE, BLACK}) {
    int s = (c == WHITE) ? 1 : -1;
    for (int f = 0; f < 8; f++) {
      int cnt = popcount(board.pieces(PAWN, c) & attacks::MASK_FILE[f]);
      if (cnt >= 2) {
        mgScore += s * -(cnt - 1) * 15;
        egScore += s * -(cnt - 1) * 20;
      }
    }
  }

  // Isolated pawn penalty
  for (Color c : {WHITE, BLACK}) {
    int s = (c == WHITE) ? 1 : -1;
    Bitboard pawns = board.pieces(PAWN, c);
    while (pawns) {
      Square sq = Square(pop_lsb(pawns));
      File f = file_of(sq);
      bool isolated = true;
      if (f > FILE_A && (board.pieces(PAWN, c) & attacks::MASK_FILE[f - 1]))
        isolated = false;
      if (f < FILE_H && (board.pieces(PAWN, c) & attacks::MASK_FILE[f + 1]))
        isolated = false;
      if (isolated) {
        mgScore += s * -20;
        egScore += s * -15;
      }
    }
  }

  // Passed pawn bonus
  for (Color c : {WHITE, BLACK}) {
    int s = (c == WHITE) ? 1 : -1;
    Bitboard pawns = board.pieces(PAWN, c);
    Bitboard enemyPawns = board.pieces(PAWN, ~c);
    while (pawns) {
      Square sq = Square(pop_lsb(pawns));
      Rank relRank = relative_rank(c, sq);
      if (relRank < RANK_2)
        continue;
      File f = file_of(sq);
      Bitboard passedMask = 0;
      int startF = std::max(0, (int)f - 1);
      int endF = std::min(7, (int)f + 1);
      for (int adjF = startF; adjF <= endF; adjF++) {
        if (c == WHITE) {
          for (int r = rank_of(sq) + 1; r <= 7; r++)
            passedMask |= attacks::MASK_FILE[adjF] & attacks::MASK_RANK[r];
        } else {
          for (int r = rank_of(sq) - 1; r >= 0; r--)
            passedMask |= attacks::MASK_FILE[adjF] & attacks::MASK_RANK[r];
        }
      }
      if ((passedMask & enemyPawns) == 0) {
        static const Value passedBonus[] = {0, 0, 10, 20, 40, 80, 160, 200};
        Value bonus = passedBonus[relRank];
        mgScore += s * bonus;
        egScore += s * bonus;
      }
    }
  }

  // King safety: pawn shelter
  for (Color c : {WHITE, BLACK}) {
    int s = (c == WHITE) ? 1 : -1;
    Square kingSq = board.kingSq(c);
    File kf = file_of(kingSq);
    Bitboard pawns = board.pieces(PAWN, c);
    int shelter = 0;
    int startF = std::max(0, (int)kf - 1);
    int endF = std::min(7, (int)kf + 1);
    for (int adjF = startF; adjF <= endF; adjF++) {
      if (c == WHITE) {
        for (int r = rank_of(kingSq) + 1; r <= std::min(7, rank_of(kingSq) + 3);
             r++) {
          if (pawns & (Bitboard(1) << make_sq((File)adjF, (Rank)r)))
            shelter += 10 - (r - rank_of(kingSq) - 1) * 3;
        }
      } else {
        for (int r = rank_of(kingSq) - 1; r >= std::max(0, rank_of(kingSq) - 3);
             r--) {
          if (pawns & (Bitboard(1) << make_sq((File)adjF, (Rank)r)))
            shelter += 10 - (rank_of(kingSq) - r - 1) * 3;
        }
      }
    }
    mgScore += s * shelter;
    egScore += s * shelter / 2;
  }

  phase = (phase * 256 + TotalPhase / 2) / TotalPhase;
  Value finalScore =
      (((mgScore * phase) + (egScore * (256 - phase))) * sign) / 256;
  return finalScore;
}
Value piece_value(PieceType pt) {
  Value pieces[] = {0,         PawnValue,  KnightValue, BishopValue,
                    RookValue, QueenValue, KingValue};
  return pieces[pt];
}
} // namespace engine::eval
