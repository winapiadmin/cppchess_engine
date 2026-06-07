#include "eval.h"
#include "tune.h"
#include <iostream>
#include <position.h>
using namespace chess;
using namespace engine::eval;

namespace engine::eval {
static Bitboard passedMask[2][64];
struct PassedMaskInit {
    PassedMaskInit() {
        for (int sq = 0; sq < 64; sq++) {
            File f = file_of(Square(sq));
            Rank r = rank_of(Square(sq));
            for (int adjF = std::max(0, (int)f - 1); adjF <= std::min(7, (int)f + 1); adjF++) {
                for (int r2 = (int)r + 1; r2 <= 7; r2++)
                    passedMask[WHITE][sq] |= attacks::MASK_FILE[adjF] & attacks::MASK_RANK[r2];
                for (int r2 = (int)r - 1; r2 >= 0; r2--)
                    passedMask[BLACK][sq] |= attacks::MASK_FILE[adjF] & attacks::MASK_RANK[r2];
            }
        }
    }
};
static PassedMaskInit passedMaskInit;
Value tempo = 50;
Value PawnValue = 72;
Value KnightValue = 372;
Value BishopValue = 202;
Value RookValue = 324;
Value QueenValue = 652;
Value fianchettoBonus = 44;
Value trappedBishopPenalty = 29;
Value centerWeight = 33;
Value mopUpKingDistWeight = 24;
Value mopUpEdgeDistWeight = 38;
Value spaceWeight = 73;
Value bishopPairMg = 94;
Value bishopPairEg = 14;
Value rookOpenFileMg = 34;
Value rookOpenFileEg = 29;
Value rookSemiOpenFileMg = 25;
Value rookSemiOpenFileEg = 14;
Value doubledPawnMg = 60;
Value doubledPawnEg = 63;
Value isolatedPawnMg = 77;
Value isolatedPawnEg = 23;
Value kingShelterBaseMg = 16;
Value kingShelterBaseEg = 6;
Value kingShelterDecayMg = 5;
Value kingShelterDecayEg = 10;
Value kqkDistWeight = 28;
Value kqkEdgeWeight = 45;
Value krkDistWeight = 4;
Value krkEdgeWeight = 17;
Value kpkWeight = 17;
Value mgMobilityCnt[7][8] = { { 92, -46, 6, -99, -98, -35, 74, -28 }, { -81, 43, 0, -100, -68, -86, 47, 29 }, { 51, 71, 88, -78, 17, -56, 99, 5 }, { -59, -78, -88, 22, 2, 86, -77, -1 }, { -55, -98, 93, 70, 27, -54, -100, -83 }, { 13, 1, -81, -7, -42, -95, -68, -99 }, { -37, 43, 8, -100, -99, 36, 91, -42 } };
Value egMobilityCnt[7][8] = { { -75, -98, 51, 26, -10, 68, 2, 10 }, { -65, -80, 95, 79, -43, -16, 100, -99 }, { 63, -25, -74, -41, 100, -37, 4, -85 }, { -66, 40, 41, 46, -65, -75, 29, 78 }, { 12, -53, -91, 23, -100, -66, 40, 73 }, { -28, 5, -46, 25, -13, 11, 87, 33 }, { -89, -4, 28, -27, 7, -1, 26, -98 } };
Value kingTropismMg[7] = { -29, 17, 7, -15, -49, -14, -1 };
Value kingTropismEg[7] = { -41, 0, -31, -41, 19, 36, 50 };
Value passedBonusMg[8] = { 313, 227, 386, 163, 109, 284, 220, 363 };
Value passedBonusEg[8] = { 380, 350, 394, 388, 149, 394, 190, 316 };
Value mg_pawn_table[56] = { 114, -495, 41, -368, -477, 412, -158, -276, -129, -129, -237, 437, -158, 121, -2, -160, 163, 459, 425, -298, 52, -296, -496, 333, -390, -473, -314, -319, -357, 348, 127, -347, 175, -346, -398, 155, 362, 477, -147, -418, -477, 311, 191, -489, -414, -352, 479, -89, 499, 265, -325, -291, -55, 491, -499, 470 };
Value mg_knight_table[64] = { 443, 51, 110, 497, 243, 173, 224, 109, -498, 280, -499, 185, 58, 494, 408, -33, -395, 45, -15, 208, 245, -13, 378, 359, -489, 352, 96, -152, 422, -414, -153, 304, -483, -113, 346, 1, 104, -185, 141, -483, 405, -500, -117, 158, -306, -145, 465, 458, 257, -344, -107, 26, 334, 369, -338, 352, -412, -223, 69, 244, -156, 335, -368, 359 };
Value mg_bishop_table[64] = { -258, -250, 404, 459, 322, 66, 411, 227, -93, -52, -352, 13, 450, -441, 392, -139, 315, 263, 210, 299, -397, 133, 371, 440, 272, 221, -76, -125, -13, 483, 135, 182, -351, 203, 101, 298, 352, 277, -478, 488, -62, 42, -144, 496, -396, 195, -414, 11, 409, 116, -373, 386, 377, 47, 493, 0, -302, 312, 81, 459, 350, -187, -265, 153 };
Value mg_rook_table[64] = { -27, -134, -499, 159, 77, -60, 266, 171, 77, -364, -338, -315, -123, 482, -341, 412, 213, 132, 134, 254, 354, -40, -145, -364, 249, 497, -490, 25, -191, -245, 104, 428, 417, 49, 499, -426, -261, 466, 80, -61, -457, 75, 95, 15, 270, -250, -171, 165, 186, 92, 401, 482, -223, -70, 384, 316, -189, 13, 185, -110, 207, -307, -257, 175 };
Value mg_king_table[64] = { -498, 290, 205, 125, -483, -313, -372, -330, 302, 312, -171, -115, 403, 43, -259, 212, -264, -396, -329, 260, -288, 374, 454, 499, -124, -366, -335, 371, 161, -361, 368, 102, -454, -133, -52, 139, -102, -418, 380, 499, -108, 340, -352, 326, 415, 2, 150, -314, 430, -161, -399, -99, 8, -292, -81, 79, -491, 309, -160, -208, -173, -283, -313, -62 };
Value mg_queen_table[64] = { -301, -182, 487, 58, 270, 273, 177, -207, -175, 246, 266, 332, -416, -363, 54, -202, 261, -423, -97, 280, -251, -265, 79, 493, 332, -133, 499, -375, -251, 210, 139, -248, 424, 193, -36, -279, 429, 62, 379, 173, 446, 0, -370, 426, -462, 287, 165, -499, 94, -425, 143, -409, 144, 495, 111, 452, 78, 157, -143, -478, 243, -97, -489, 112 };
Value eg_pawn_table[56] = { -273, -6, 100, 320, -127, 163, -33, 79, 235, 334, 47, 435, -149, -1, -155, -277, -25, 220, 340, -396, 401, 446, -30, 424, 472, -385, 486, -91, 99, -275, 340, -330, -125, 150, -121, -30, -207, -244, -277, 411, 301, -298, 77, -313, -141, -357, 386, 255, -495, 80, -500, 400, -450, 130, -111, -28 };
Value eg_knight_table[64] = { 371, -385, 222, -192, 139, 46, -343, 59, 366, -360, 248, 384, -63, -425, 225, 414, -333, 396, -421, -99, 10, 367, -368, 500, -500, 270, 41, 168, 154, 327, -161, 457, -368, 79, -265, -457, 94, -294, -234, 348, 233, -471, -360, 380, -228, 3, 314, 415, -327, 200, -91, 251, -390, -294, -187, -335, -29, 177, -341, 197, -37, 143, 217, 384 };
Value eg_bishop_table[64] = { 31, 57, -369, -271, 267, -495, -141, -276, 239, 405, 217, 34, -391, -36, 122, -427, -35, 406, 286, 52, 497, 162, 262, -147, -251, -470, -258, -171, -63, -203, 296, 497, 90, 444, 422, -241, -499, 62, -312, 299, -345, 309, 168, 142, 161, -170, 138, -116, 359, -45, 214, 191, -360, 163, 433, 386, 182, -181, 348, 258, 415, 375, -233, 214 };
Value eg_rook_table[64] = { -227, 492, 17, -205, 495, 12, 207, -246, 195, -22, 312, 16, -124, -259, 405, -494, -262, -344, -218, -12, 8, 388, 306, 316, 376, 455, -139, -194, 41, 158, 283, 476, 469, 94, 63, -498, -414, 0, -292, -373, -270, 256, -162, -385, 453, -220, 162, -53, -221, -435, -426, -222, 235, 116, 182, 409, -327, -486, 80, 109, -455, -247, 106, -307 };
Value eg_king_table[64] = { -319, -481, 495, -290, 1, 426, -347, -112, 423, -89, 500, -432, 54, -223, 148, 118, -292, -351, -205, 195, 377, -185, -296, -176, -73, 247, -491, -411, -232, 106, -295, 326, 60, 500, -488, 218, 207, -409, 91, -390, 35, -249, 111, -179, -169, -182, -372, -330, 425, 455, 457, 500, -494, -442, 24, 170, 367, 353, -297, 97, -137, 125, 221, -32 };
Value eg_queen_table[64] = { -326, 119, 358, 91, -311, -289, 373, 329, 44, -83, 162, -470, -470, -2, 45, -486, 438, 39, -493, 371, 493, -278, 338, -416, 360, 499, 357, -145, -500, 398, -210, -199, 218, -22, -367, 69, 144, 488, -55, -71, 499, -36, -262, -351, -407, 291, -497, -412, -164, -46, 186, 323, -56, -416, -129, 394, 71, -343, 183, -360, 91, -295, -180, 88 };

Value *mgPst[] = { nullptr,       mg_pawn_table,  mg_knight_table, mg_bishop_table,
                   mg_rook_table, mg_queen_table, mg_king_table };

Value *egPst[] = { nullptr,       eg_pawn_table,  eg_knight_table, eg_bishop_table,
                   eg_rook_table, eg_queen_table, eg_king_table };

// tuning slop here
TUNE(SetRange(0, 50), tempo, SetRange(50, 200), PawnValue, SetRange(200, 500), KnightValue,
     SetRange(200, 500), BishopValue, SetRange(300, 700), RookValue, SetRange(600, 1400), QueenValue);
TUNE(SetRange(-100, 100), mgMobilityCnt, egMobilityCnt);
TUNE(SetRange(0, 50), fianchettoBonus, SetRange(0, 150), trappedBishopPenalty);
TUNE(SetRange(-50, 50), kingTropismMg, kingTropismEg);
TUNE(SetRange(0, 50), centerWeight, SetRange(0, 30), mopUpKingDistWeight,
     SetRange(0, 50), mopUpEdgeDistWeight, SetRange(0, 100), spaceWeight);
TUNE(SetRange(0, 100), bishopPairMg, SetRange(0, 100), bishopPairEg);
TUNE(SetRange(0, 50), rookOpenFileMg, SetRange(0, 50), rookOpenFileEg,
     SetRange(0, 50), rookSemiOpenFileMg, SetRange(0, 50), rookSemiOpenFileEg);
TUNE(SetRange(0, 100), doubledPawnMg, SetRange(0, 100), doubledPawnEg,
     SetRange(0, 100), isolatedPawnMg, SetRange(0, 100), isolatedPawnEg);
TUNE(SetRange(0, 400), passedBonusMg, passedBonusEg);
TUNE(SetRange(0, 30), kingShelterBaseMg, SetRange(0, 30), kingShelterBaseEg,
     SetRange(0, 10), kingShelterDecayMg, SetRange(0, 10), kingShelterDecayEg);
TUNE(SetRange(-500, 500), mg_pawn_table, mg_knight_table, mg_bishop_table,
     mg_rook_table, mg_king_table, mg_queen_table,
     eg_pawn_table, eg_knight_table, eg_bishop_table,
     eg_rook_table, eg_king_table, eg_queen_table);
TUNE(SetRange(0, 50), kqkDistWeight, SetRange(0, 50), kqkEdgeWeight,
     SetRange(0, 50), krkDistWeight, SetRange(0, 50), krkEdgeWeight,
     SetRange(0, 50), kpkWeight);

Value eval(const chess::Board &board) {
    constexpr int KnightPhase = 1;
    constexpr int BishopPhase = 1;
    constexpr int RookPhase = 2;
    constexpr int QueenPhase = 4;
    constexpr int TotalPhase = KnightPhase * 4 + BishopPhase * 4 + RookPhase * 4 + QueenPhase * 2;
    const int sign = board.side_to_move() == WHITE ? 1 : -1;
    int mgScore = 0;
    int egScore = 0;
    int phase = 0;
    {
        Bitboard pinMask = board.pin_mask();
        Bitboard occ = board.occ(), occ2 = occ;
        while (occ) {
            Square sq = Square(pop_lsb(occ));
            auto p = board.at(sq);
            Color pc = color_of(p);
            int _sign = pc == WHITE ? 1 : -1;
            Square _sq = _sign == 1 ? sq : square_mirror(sq);
            auto pt = piece_of(p);
            if (pt == NO_PIECE_TYPE)
                continue;
            mgScore += _sign * mgPst[pt][_sq];
            egScore += _sign * egPst[pt][_sq];
            mgScore += _sign * piece_value(pt);
            egScore += _sign * piece_value(pt);
            if (pt == KNIGHT) phase += KnightPhase;
            else if (pt == BISHOP) phase += BishopPhase;
            else if (pt == ROOK) phase += RookPhase;
            else if (pt == QUEEN) phase += QueenPhase;
            Bitboard attacks = 0;
            if (pt == KNIGHT) attacks = chess::attacks::knight(sq);
            else if (pt == BISHOP) attacks = chess::attacks::bishop(sq, occ2);
            else if (pt == ROOK) attacks = chess::attacks::rook(sq, occ2);
            else if (pt == QUEEN) attacks = chess::attacks::queen(sq, occ2);
            attacks &= ~board.us(pc);
            int mobility = popcount(attacks);
            int clampedMobility = std::min(mobility, 7);
            if ((1ULL << sq) & pinMask)
                clampedMobility = std::min(clampedMobility / 4, 7);
            mgScore += _sign * mgMobilityCnt[pt][clampedMobility];
            egScore += _sign * egMobilityCnt[pt][clampedMobility];
            if (pt != PAWN && pt != KING) {
                int kd = square_distance(sq, board.kingSq(~pc));
                mgScore += _sign * (7 - kd) * kingTropismMg[pt];
                egScore += _sign * (7 - kd) * kingTropismEg[pt];
            }
        }
    }
    // Bishop pair bonus
    for (Color c : { WHITE, BLACK }) {
        if (board.count(BISHOP, c) >= 2) {
            int s = (c == WHITE) ? 1 : -1;
            mgScore += s * bishopPairMg;
            egScore += s * bishopPairEg;
        }
    }

    // Fianchetto bonus
    for (Color c : { WHITE, BLACK }) {
        int s = (c == WHITE) ? 1 : -1;
        Square fianchettoSq[2] = { relative_square(c, SQ_B2), relative_square(c, SQ_G2) };
        Square pawnSq[2] = { relative_square(c, SQ_B3), relative_square(c, SQ_G3) };
        for (int i = 0; i < 2; i++) {
            if (board.at<PieceType>(fianchettoSq[i]) == BISHOP && board.at<Color>(fianchettoSq[i]) == c
                && board.at<PieceType>(pawnSq[i]) == PAWN && board.at<Color>(pawnSq[i]) == c) {
                mgScore += s * fianchettoBonus;
                egScore += s * fianchettoBonus;
            }
        }
    }

    // Trapped bishop penalty
    for (Color c : { WHITE, BLACK }) {
        int s = (c == WHITE) ? 1 : -1;
        Square trapSq[2] = { relative_square(c, SQ_A2), relative_square(c, SQ_H2) };
        Square kingAdj[2] = { relative_square(c, SQ_B1), relative_square(c, SQ_G1) };
        for (int i = 0; i < 2; i++) {
            if (board.at<PieceType>(trapSq[i]) == BISHOP && board.at<Color>(trapSq[i]) == c
                && board.kingSq(c) == kingAdj[i]) {
                mgScore -= s * trappedBishopPenalty;
                egScore -= s * trappedBishopPenalty;
            }
        }
    }

    // Rook on open/semi-open file
    for (Color c : { WHITE, BLACK }) {
        int s = (c == WHITE) ? 1 : -1;
        Bitboard rooks = board.pieces(ROOK, c);
        while (rooks) {
            Square sq = Square(pop_lsb(rooks));
            File f = file_of(sq);
            Bitboard fileMask = attacks::MASK_FILE[f];
            bool hasOwnPawn = (board.pieces(PAWN, c) & fileMask) != 0;
            bool hasEnemyPawn = (board.pieces(PAWN, ~c) & fileMask) != 0;
            if (!hasOwnPawn && !hasEnemyPawn) {
                mgScore += s * rookOpenFileMg;
                egScore += s * rookOpenFileEg;
            } else if (!hasOwnPawn) {
                mgScore += s * rookSemiOpenFileMg;
                egScore += s * rookSemiOpenFileEg;
            }
        }
    }

    // Precompute pawn data
    Bitboard pawnBB[2] = { board.pieces(PAWN, WHITE), board.pieces(PAWN, BLACK) };
    Bitboard pawnAtks[2] = { attacks::pawn<WHITE>(pawnBB[WHITE]), attacks::pawn<BLACK>(pawnBB[BLACK]) };

    // Pawn structure: doubled, isolated, passed in one pass per color
    for (Color c : { WHITE, BLACK }) {
        int s = (c == WHITE) ? 1 : -1;
        Bitboard pawns = pawnBB[c];
        Bitboard enemyPawns = pawnBB[~c];

        bool fileHasPawn[8] = {false};
        int fileCount[8] = {0};
        Bitboard tmp = pawns;
        while (tmp) {
            Square sq = Square(pop_lsb(tmp));
            File f = file_of(sq);
            fileCount[f]++;
            fileHasPawn[f] = true;
        }

        for (int f = 0; f < 8; f++)
            if (fileCount[f] >= 2) {
                mgScore -= s * (fileCount[f] - 1) * doubledPawnMg;
                egScore -= s * (fileCount[f] - 1) * doubledPawnEg;
            }

        tmp = pawns;
        while (tmp) {
            Square sq = Square(pop_lsb(tmp));
            File f = file_of(sq);
            Rank relRank = relative_rank(c, sq);

            bool isolated = true;
            if (f > FILE_A && fileHasPawn[f - 1]) isolated = false;
            if (f < FILE_H && fileHasPawn[f + 1]) isolated = false;
            if (isolated) {
                mgScore -= s * isolatedPawnMg;
                egScore -= s * isolatedPawnEg;
            }

            if (relRank >= RANK_2 && (passedMask[c][sq] & enemyPawns) == 0) {
                mgScore += s * passedBonusMg[relRank];
                egScore += s * passedBonusEg[relRank];
            }
        }
    }

    // Center control (using precomputed pawn attacks)
    {
        Bitboard centerMask = (1ULL << SQ_D4) | (1ULL << SQ_E4) | (1ULL << SQ_D5) | (1ULL << SQ_E5);
        int wc = popcount(pawnAtks[WHITE] & centerMask);
        int bc = popcount(pawnAtks[BLACK] & centerMask);
        mgScore += (wc - bc) * centerWeight;
        egScore += (wc - bc) * centerWeight;
    }

    // Space evaluation
    for (Color c : { WHITE, BLACK }) {
        int s = (c == WHITE) ? 1 : -1;
        Bitboard safe = ~pawnAtks[~c];
        Bitboard enemyCamp = c == WHITE
            ? (attacks::MASK_RANK[4] | attacks::MASK_RANK[5] | attacks::MASK_RANK[6])
            : (attacks::MASK_RANK[3] | attacks::MASK_RANK[2] | attacks::MASK_RANK[1]);
        int space = popcount(pawnAtks[c] & safe & enemyCamp);
        mgScore += s * space * spaceWeight;
        egScore += s * space * spaceWeight;
    }

    // King safety: pawn shelter
    for (Color c : { WHITE, BLACK }) {
        int s = (c == WHITE) ? 1 : -1;
        Square kingSq = board.kingSq(c);
        File kf = file_of(kingSq);
        Rank kr = rank_of(kingSq);
        Bitboard pawns = board.pieces(PAWN, c);
        int shelterMg = 0, shelterEg = 0;
        int startF = std::max(0, (int)kf - 1);
        int endF = std::min(7, (int)kf + 1);
        for (int adjF = startF; adjF <= endF; adjF++) {
            Bitboard fileMask = attacks::MASK_FILE[adjF];
            if (c == WHITE) {
                for (int r = (int)kr + 1; r <= std::min(7, (int)kr + 3); r++) {
                    if (pawns & (fileMask & attacks::MASK_RANK[r])) {
                        shelterMg += kingShelterBaseMg - (r - (int)kr - 1) * kingShelterDecayMg;
                        shelterEg += kingShelterBaseEg - (r - (int)kr - 1) * kingShelterDecayEg;
                    }
                }
            } else {
                for (int r = (int)kr - 1; r >= std::max(0, (int)kr - 3); r--) {
                    if (pawns & (fileMask & attacks::MASK_RANK[r])) {
                        shelterMg += kingShelterBaseMg - ((int)kr - r - 1) * kingShelterDecayMg;
                        shelterEg += kingShelterBaseEg - ((int)kr - r - 1) * kingShelterDecayEg;
                    }
                }
            }
        }
        mgScore += s * shelterMg;
        egScore += s * shelterEg;
    }

    // Endgame bonuses + mop-up in one pass per color
    for (Color c : { WHITE, BLACK }) {
        int s = (c == WHITE) ? 1 : -1;
        int oppCount = popcount(board.occ(~c));
        Square myKing = board.kingSq(c);
        Square oppKing = board.kingSq(~c);

        // KQK: queen vs lone king
        if (board.count(QUEEN, c) >= 1 && oppCount == 1) {
            int kingDist = std::max(std::abs((int)file_of(myKing) - (int)file_of(oppKing)),
                                    std::abs((int)rank_of(myKing) - (int)rank_of(oppKing)));
            File ef = file_of(oppKing);
            Rank er = rank_of(oppKing);
            int edgeDist = std::min(std::min((int)ef, 7 - (int)ef), std::min((int)er, 7 - (int)er));
            mgScore += s * ((14 - kingDist) * kqkDistWeight + (7 - edgeDist) * kqkEdgeWeight);
            egScore += s * ((14 - kingDist) * kqkDistWeight + (7 - edgeDist) * kqkEdgeWeight);
        }

        // KRK: rook vs lone king
        if (board.count(ROOK, c) >= 1 && oppCount == 1
            && board.count(QUEEN, c) == 0 && board.count(BISHOP, c) == 0
            && board.count(KNIGHT, c) == 0 && board.count(PAWN, c) == 0) {
            int kingDist = std::max(std::abs((int)file_of(myKing) - (int)file_of(oppKing)),
                                    std::abs((int)rank_of(myKing) - (int)rank_of(oppKing)));
            File ef = file_of(oppKing);
            Rank er = rank_of(oppKing);
            int edgeDist = std::min(std::min((int)ef, 7 - (int)ef), std::min((int)er, 7 - (int)er));
            mgScore += s * ((14 - kingDist) * krkDistWeight + (7 - edgeDist) * krkEdgeWeight);
            egScore += s * ((14 - kingDist) * krkDistWeight + (7 - edgeDist) * krkEdgeWeight);
        }

        // KPK: king + pawn(s) vs lone king
        if (board.count(PAWN, c) >= 1 && oppCount == 1
            && board.count(QUEEN, c) == 0 && board.count(ROOK, c) == 0
            && board.count(BISHOP, c) == 0 && board.count(KNIGHT, c) == 0) {
            Bitboard ps = pawnBB[c];
            while (ps) {
                Square psq = Square(pop_lsb(ps));
                Rank pr = relative_rank(c, psq);
                if (pr < RANK_4) continue;

                File pf = file_of(psq);
                Square promSq = make_sq(pf, c == WHITE ? RANK_8 : RANK_1);
                int pawnDist = (c == WHITE) ? (7 - (int)rank_of(psq)) : (int)rank_of(psq);
                int pkDist = std::max(std::abs((int)file_of(oppKing) - (int)pf),
                                      std::abs((int)rank_of(oppKing) - (int)rank_of(promSq)));
                int mkDist = std::max(std::abs((int)file_of(myKing) - (int)file_of(psq)),
                                      std::abs((int)rank_of(myKing) - (int)rank_of(psq)));

                bool winning = pkDist > pawnDist
                    || mkDist <= pawnDist + 1
                    || (pr >= RANK_6 && file_of(myKing) == pf
                        && ((c == WHITE && rank_of(myKing) >= RANK_6)
                            || (c == BLACK && rank_of(myKing) <= RANK_3)));
                if ((pf == FILE_A || pf == FILE_H) && oppKing == promSq)
                    winning = false;

                if (winning) {
                    mgScore += s * kpkWeight * pawnDist;
                    egScore += s * kpkWeight * pawnDist;
                }
            }
        }

        // Mop-up: general endgame drive
        Bitboard ourMat = board.occ(c) & ~board.pieces(PAWN) & ~board.pieces(KING);
        Bitboard theirMat = board.occ(~c) & ~board.pieces(PAWN) & ~board.pieces(KING);
        if (ourMat && !theirMat && oppCount <= 2) {
            int kingDist = std::max(std::abs((int)file_of(myKing) - (int)file_of(oppKing)),
                                    std::abs((int)rank_of(myKing) - (int)rank_of(oppKing)));
            File ef = file_of(oppKing);
            Rank er = rank_of(oppKing);
            int edgeDist = std::min(std::min((int)ef, 7 - (int)ef), std::min((int)er, 7 - (int)er));
            mgScore += s * ((14 - kingDist) * mopUpKingDistWeight + (7 - edgeDist) * mopUpEdgeDistWeight);
            egScore += s * ((14 - kingDist) * mopUpKingDistWeight + (7 - edgeDist) * mopUpEdgeDistWeight);
        }
    }

    // Draw detection: score 0 for positions where neither side can force a win
    int totalPieces = popcount(board.occ());
    int pawnCount = board.count<PAWN>();

    // KBKB same-colored bishops (no pawns) - drawn
    if (totalPieces == 4 && pawnCount == 0 && board.count<BISHOP>() == 2
        && board.count<KNIGHT>() == 0 && board.count<ROOK>() == 0 && board.count<QUEEN>() == 0
        && board.count(BISHOP, WHITE) == 1 && board.count(BISHOP, BLACK) == 1) {
        Bitboard wbBB = board.pieces(BISHOP, WHITE);
        Bitboard bbBB = board.pieces(BISHOP, BLACK);
        Square wb = Square(pop_lsb(wbBB));
        Square bb = Square(pop_lsb(bbBB));
        if (square_color(wb) == square_color(bb))
            return 0;
    }

    // KNNK (no pawns) - drawn
    if (totalPieces == 4 && pawnCount == 0 && board.count<KNIGHT>() == 2
        && board.count<BISHOP>() == 0 && board.count<ROOK>() == 0 && board.count<QUEEN>() == 0)
        return 0;

    phase = (phase * 256 + TotalPhase / 2) / TotalPhase;
    Value finalScore = (((mgScore * phase) + (egScore * (256 - phase))) * sign) / 256 + tempo;
    return finalScore;
}
Value piece_value(PieceType pt) {
    Value pieces[] = { 0, PawnValue, KnightValue, BishopValue, RookValue, QueenValue, 0 };
    return pieces[pt];
}
} // namespace engine::eval
