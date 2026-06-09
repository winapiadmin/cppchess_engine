#include "eval.h"
#include "Weights.h"
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

Value *mgPst[] = { nullptr, mg_pawn_table, mg_knight_table, mg_bishop_table, mg_rook_table, mg_queen_table, mg_king_table };

Value *egPst[] = { nullptr, eg_pawn_table, eg_knight_table, eg_bishop_table, eg_rook_table, eg_queen_table, eg_king_table };

// tuning slop here
TUNE(SetRange(5, 30),
     tempo,
     SetRange(80, 120),
     PawnValue,
     SetRange(280, 370),
     KnightValue,
     SetRange(300, 400),
     BishopValue,
     SetRange(450, 550),
     RookValue,
     SetRange(800, 1000),
     QueenValue);
TUNE(SetRange(-10, 10), mgMobilityCnt, egMobilityCnt);
TUNE(SetRange(0, 30), fianchettoBonus, SetRange(0, 100), trappedBishopPenalty);
TUNE(SetRange(-20, 20), kingTropismMg, kingTropismEg);
TUNE(SetRange(0, 30),
     centerWeight,
     SetRange(0, 20),
     mopUpKingDistWeight,
     SetRange(0, 20),
     mopUpEdgeDistWeight,
     SetRange(1, 30),
     spaceWeight);
TUNE(SetRange(0, 50), bishopPairMg, SetRange(0, 50), bishopPairEg);
TUNE(SetRange(0, 30),
     rookOpenFileMg,
     SetRange(0, 30),
     rookOpenFileEg,
     SetRange(0, 20),
     rookSemiOpenFileMg,
     SetRange(0, 20),
     rookSemiOpenFileEg);
TUNE(SetRange(0, 30),
     doubledPawnMg,
     SetRange(0, 30),
     doubledPawnEg,
     SetRange(0, 40),
     isolatedPawnMg,
     SetRange(0, 40),
     isolatedPawnEg);
TUNE(SetRange(0, 200),
     passedBonusMg[1],
     passedBonusMg[2],
     passedBonusMg[3],
     passedBonusMg[4],
     passedBonusMg[5],
     passedBonusMg[6],
     passedBonusEg[1],
     passedBonusEg[2],
     passedBonusEg[3],
     passedBonusEg[4],
     passedBonusEg[5],
     passedBonusEg[6]);
TUNE(SetRange(1, 30),
     kingShelterBaseMg,
     SetRange(1, 30),
     kingShelterBaseEg,
     SetRange(1, 10),
     kingShelterDecayMg,
     SetRange(1, 10),
     kingShelterDecayEg);
TUNE(SetRange(-25, 25),
     mg_knight_table,
     mg_bishop_table,
     mg_rook_table,
     mg_king_table,
     mg_queen_table,
     eg_knight_table,
     eg_bishop_table,
     eg_rook_table,
     eg_king_table,
     eg_queen_table,
     mg_pawn_table[8],
     eg_pawn_table[8],
     mg_pawn_table[9],
     eg_pawn_table[9],
     mg_pawn_table[10],
     eg_pawn_table[10],
     mg_pawn_table[11],
     eg_pawn_table[11],
     mg_pawn_table[12],
     eg_pawn_table[12],
     mg_pawn_table[13],
     eg_pawn_table[13],
     mg_pawn_table[14],
     eg_pawn_table[14],
     mg_pawn_table[15],
     eg_pawn_table[15],
     mg_pawn_table[16],
     eg_pawn_table[16],
     mg_pawn_table[17],
     eg_pawn_table[17],
     mg_pawn_table[18],
     eg_pawn_table[18],
     mg_pawn_table[19],
     eg_pawn_table[19],
     mg_pawn_table[20],
     eg_pawn_table[20],
     mg_pawn_table[21],
     eg_pawn_table[21],
     mg_pawn_table[22],
     eg_pawn_table[22],
     mg_pawn_table[23],
     eg_pawn_table[23],
     mg_pawn_table[24],
     eg_pawn_table[24],
     mg_pawn_table[25],
     eg_pawn_table[25],
     mg_pawn_table[26],
     eg_pawn_table[26],
     mg_pawn_table[27],
     eg_pawn_table[27],
     mg_pawn_table[28],
     eg_pawn_table[28],
     mg_pawn_table[29],
     eg_pawn_table[29],
     mg_pawn_table[30],
     eg_pawn_table[30],
     mg_pawn_table[31],
     eg_pawn_table[31],
     mg_pawn_table[32],
     eg_pawn_table[32],
     mg_pawn_table[33],
     eg_pawn_table[33],
     mg_pawn_table[34],
     eg_pawn_table[34],
     mg_pawn_table[35],
     eg_pawn_table[35],
     mg_pawn_table[36],
     eg_pawn_table[36],
     mg_pawn_table[37],
     eg_pawn_table[37],
     mg_pawn_table[38],
     eg_pawn_table[38],
     mg_pawn_table[39],
     eg_pawn_table[39],
     mg_pawn_table[40],
     eg_pawn_table[40],
     mg_pawn_table[41],
     eg_pawn_table[41],
     mg_pawn_table[42],
     eg_pawn_table[42],
     mg_pawn_table[43],
     eg_pawn_table[43],
     mg_pawn_table[44],
     eg_pawn_table[44],
     mg_pawn_table[45],
     eg_pawn_table[45],
     mg_pawn_table[46],
     eg_pawn_table[46],
     mg_pawn_table[47],
     eg_pawn_table[47],
     mg_pawn_table[48],
     eg_pawn_table[48],
     mg_pawn_table[49],
     eg_pawn_table[49],
     mg_pawn_table[50],
     eg_pawn_table[50],
     mg_pawn_table[51],
     eg_pawn_table[51],
     mg_pawn_table[52],
     eg_pawn_table[52],
     mg_pawn_table[53],
     eg_pawn_table[53],
     mg_pawn_table[54],
     eg_pawn_table[54],
     mg_pawn_table[55],
     eg_pawn_table[55]);
TUNE(SetRange(0, 30),
     kqkDistWeight,
     SetRange(0, 20),
     kqkEdgeWeight,
     SetRange(0, 20),
     krkDistWeight,
     SetRange(0, 20),
     krkEdgeWeight,
     SetRange(0, 30),
     kpkWeight);

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
            if (pt == KNIGHT)
                phase += KnightPhase;
            else if (pt == BISHOP)
                phase += BishopPhase;
            else if (pt == ROOK)
                phase += RookPhase;
            else if (pt == QUEEN)
                phase += QueenPhase;
            Bitboard attacks = 0;
            if (pt == KNIGHT)
                attacks = chess::attacks::knight(sq);
            else if (pt == BISHOP)
                attacks = chess::attacks::bishop(sq, occ2);
            else if (pt == ROOK)
                attacks = chess::attacks::rook(sq, occ2);
            else if (pt == QUEEN)
                attacks = chess::attacks::queen(sq, occ2);
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
            if (board.at<PieceType>(fianchettoSq[i]) == BISHOP && board.at<Color>(fianchettoSq[i]) == c &&
                board.at<PieceType>(pawnSq[i]) == PAWN && board.at<Color>(pawnSq[i]) == c) {
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
            if (board.at<PieceType>(trapSq[i]) == BISHOP && board.at<Color>(trapSq[i]) == c && board.kingSq(c) == kingAdj[i]) {
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

        bool fileHasPawn[8] = { false };
        int fileCount[8] = { 0 };
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
            if (f > FILE_A && fileHasPawn[f - 1])
                isolated = false;
            if (f < FILE_H && fileHasPawn[f + 1])
                isolated = false;
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
        Bitboard enemyCamp = c == WHITE ? (attacks::MASK_RANK[4] | attacks::MASK_RANK[5] | attacks::MASK_RANK[6])
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
        if (board.count(ROOK, c) >= 1 && oppCount == 1 && board.count(QUEEN, c) == 0 && board.count(BISHOP, c) == 0 &&
            board.count(KNIGHT, c) == 0 && board.count(PAWN, c) == 0) {
            int kingDist = std::max(std::abs((int)file_of(myKing) - (int)file_of(oppKing)),
                                    std::abs((int)rank_of(myKing) - (int)rank_of(oppKing)));
            File ef = file_of(oppKing);
            Rank er = rank_of(oppKing);
            int edgeDist = std::min(std::min((int)ef, 7 - (int)ef), std::min((int)er, 7 - (int)er));
            mgScore += s * ((14 - kingDist) * krkDistWeight + (7 - edgeDist) * krkEdgeWeight);
            egScore += s * ((14 - kingDist) * krkDistWeight + (7 - edgeDist) * krkEdgeWeight);
        }

        // KPK: king + pawn(s) vs lone king
        if (board.count(PAWN, c) >= 1 && oppCount == 1 && board.count(QUEEN, c) == 0 && board.count(ROOK, c) == 0 &&
            board.count(BISHOP, c) == 0 && board.count(KNIGHT, c) == 0) {
            Bitboard ps = pawnBB[c];
            while (ps) {
                Square psq = Square(pop_lsb(ps));
                Rank pr = relative_rank(c, psq);
                if (pr < RANK_4)
                    continue;

                File pf = file_of(psq);
                Square promSq = make_sq(pf, c == WHITE ? RANK_8 : RANK_1);
                int pawnDist = (c == WHITE) ? (7 - (int)rank_of(psq)) : (int)rank_of(psq);
                int pkDist =
                    std::max(std::abs((int)file_of(oppKing) - (int)pf), std::abs((int)rank_of(oppKing) - (int)rank_of(promSq)));
                int mkDist = std::max(std::abs((int)file_of(myKing) - (int)file_of(psq)),
                                      std::abs((int)rank_of(myKing) - (int)rank_of(psq)));

                bool winning = pkDist > pawnDist || mkDist <= pawnDist + 1 ||
                               (pr >= RANK_6 && file_of(myKing) == pf &&
                                ((c == WHITE && rank_of(myKing) >= RANK_6) || (c == BLACK && rank_of(myKing) <= RANK_3)));
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
    if (totalPieces == 4 && pawnCount == 0 && board.count<BISHOP>() == 2 && board.count<KNIGHT>() == 0 &&
        board.count<ROOK>() == 0 && board.count<QUEEN>() == 0 && board.count(BISHOP, WHITE) == 1 &&
        board.count(BISHOP, BLACK) == 1) {
        Bitboard wbBB = board.pieces(BISHOP, WHITE);
        Bitboard bbBB = board.pieces(BISHOP, BLACK);
        Square wb = Square(pop_lsb(wbBB));
        Square bb = Square(pop_lsb(bbBB));
        if (square_color(wb) == square_color(bb))
            return 0;
    }

    // KNNK (no pawns) - drawn
    if (totalPieces == 4 && pawnCount == 0 && board.count<KNIGHT>() == 2 && board.count<BISHOP>() == 0 &&
        board.count<ROOK>() == 0 && board.count<QUEEN>() == 0)
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
