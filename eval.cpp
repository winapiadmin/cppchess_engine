#include "eval.hpp"
#include <cstring>
#define EvaluationResult int16_t
// 0. Evaluation result

struct EvalBreakdown
{
    EvaluationResult phase;
    EvaluationResult Material;
    EvaluationResult Doubled;
    EvaluationResult Isolated;
    EvaluationResult Backward;
    EvaluationResult Passed;
    EvaluationResult Center;
    EvaluationResult Mobility;
    EvaluationResult Shield;
    EvaluationResult KingTropism;
    EvaluationResult Space;
    EvaluationResult MGPSQT;
    EvaluationResult EGPSQT;
};

EvalBreakdown wEval{}, bEval{};
// 1. Weights, in case of tuning (centipawns)
// 1.1. Material weights
constexpr int16_t PawnValue = 100, KnightValue = 325, BishopValue = 350, RookValue = 500, QueenValue = 900;
// 1.2. Pawn structure
constexpr int16_t Doubled = -100, Isolated = -80, Backward = -100, wpassed = 100, cpassed = 50,
                  Center = 100;
// 1.3. Mobility

// Mobility weights per piece type
constexpr int MOBILITY_WEIGHTS[] = {
    0, // EMPTY / None
    1, // PAWN (often ignored or handled separately)
    4, // KNIGHT
    4, // BISHOP
    6, // ROOK
    8, // QUEEN
    0  // KING (usually ignored)
};
// 1.3. King safety
constexpr int16_t pawnShield = 20, KingTropism = 5;
// 1.4. Space
constexpr int16_t Space = 10;
// 1.5. Tempo
constexpr int16_t Tempo = 28;
// 1.6. PQST (source: Chessprogramming Wiki)
// clang-format off
int16_t mg_pawn_table[64] = {
        0,   0,   0,   0,   0,   0,  0,   0,
        98, 134,  61,  95,  68, 126, 34, -11,
        -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
        0,   0,   0,   0,   0,   0,  0,   0,
};

int16_t eg_pawn_table[64] = {
        0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
        94, 100,  85,  67,  56,  53,  82,  84,
        32,  24,  13,   5,  -2,   4,  17,  17,
        13,   9,  -3,  -7,  -7,  -8,   3,  -1,
        4,   7,  -6,   1,   0,  -5,  -1,  -8,
        13,   8,   8,  10,  13,   0,   2,  -7,
        0,   0,   0,   0,   0,   0,   0,   0,
};

int16_t mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
        -73, -41,  72,  36,  23,  62,   7,  -17,
        -47,  60,  37,  65,  84, 129,  73,   44,
        -9,  17,  19,  53,  37,  69,  18,   22,
        -13,   4,  16,  13,  28,  19,  21,   -8,
        -23,  -9,  12,  10,  19,  17,  25,  -16,
        -29, -53, -12,  -3,  -1,  18, -14,  -19,
        -105, -21, -58, -33, -17, -28, -19,  -23,
};

int16_t eg_knight_table[64] = {
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -15, -22, -18, -50, -64,
};

int16_t mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
        -4,   5,  19,  50,  37,  37,   7,  -2,
        -6,  13,  13,  26,  34,  12,  10,   4,
        0,  15,  15,  15,  14,  27,  18,  10,
        4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

int16_t eg_bishop_table[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
        -8,  -4,   7, -12, -3, -13,  -4, -14,
        2,  -8,   0,  -1, -2,   6,   0,   4,
        -3,   9,  12,   9, 14,  10,   3,   2,
        -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int16_t mg_rook_table[64] = {
        32,  42,  32,  51, 63,  9,  31,  43,
        27,  32,  58,  62, 80, 67,  26,  44,
        -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

int16_t eg_rook_table[64] = {
        13, 10, 18, 15, 12,  12,   8,   5,
        11, 13, 13, 11, -3,   3,   8,   3,
        7,  7,  7,  5,  4,  -3,  -5,  -3,
        4,  3, 13,  1,  2,   1,  -1,   2,
        3,  5,  8,  4, -5,  -6,  -8, -11,
        -4,  0, -5, -1, -7, -12,  -8, -16,
        -6, -6,  0,  2, -9,  -9, -11,  -3,
        -9,  2,  3, -1, -5, -13,   4, -20,
};

int16_t mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
        -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
        -1, -18,  -9,  10, -15, -25, -31, -50,
};

int16_t eg_queen_table[64] = {
        -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
        3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

int16_t mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
        29,  -1, -20,  -7,  -8,  -4, -38, -29,
        -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
        1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

int16_t eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
        10,  17,  23,  15,  20,  45,  44,  13,
        -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

int16_t* mg_pesto_table[6] =
{
    mg_pawn_table,
    mg_knight_table,
    mg_bishop_table,
    mg_rook_table,
    mg_queen_table,
    mg_king_table
};

int16_t* eg_pesto_table[6] =
{
    eg_pawn_table,
    eg_knight_table,
    eg_bishop_table,
    eg_rook_table,
    eg_queen_table,
    eg_king_table
};

// clang-format on
// 2. Evaluation functions
// 2.1. Phase
Evaluation int phase(const chess::Board &pos)
{
    constexpr int KnightPhase = 1;
    constexpr int BishopPhase = 1;
    constexpr int RookPhase = 2;
    constexpr int QueenPhase = 4;
    constexpr int TotalPhase = KnightPhase * 4 + BishopPhase * 4 + RookPhase * 4 + QueenPhase * 2;

    int phase = (pos.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).count() +
                 pos.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK).count()) *
                    KnightPhase +
                (pos.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).count() +
                 pos.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).count()) *
                    BishopPhase +
                (pos.pieces(chess::PieceType::ROOK, chess::Color::WHITE).count() +
                 pos.pieces(chess::PieceType::ROOK, chess::Color::BLACK).count()) *
                    RookPhase +
                (pos.pieces(chess::PieceType::QUEEN, chess::Color::WHITE).count() +
                 pos.pieces(chess::PieceType::QUEEN, chess::Color::BLACK).count()) *
                    QueenPhase;

    return wEval.phase = (phase * 256 + TotalPhase / 2) / TotalPhase;
}
// 2.2. Material (one-line) (all phases)
AllPhases inline int16_t material(const chess::Board &pos)
{ // Precompute piece counts (avoid multiple function calls)
    int pieceCount[10] = {
        pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE).count(),
        pos.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).count(),
        pos.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).count(),
        pos.pieces(chess::PieceType::ROOK, chess::Color::WHITE).count(),
        pos.pieces(chess::PieceType::QUEEN, chess::Color::WHITE).count(),
        pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK).count(),
        pos.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK).count(),
        pos.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).count(),
        pos.pieces(chess::PieceType::ROOK, chess::Color::BLACK).count(),
        pos.pieces(chess::PieceType::QUEEN, chess::Color::BLACK).count(),
    };
    wEval.Material = pieceCount[0] * PawnValue + pieceCount[1] * KnightValue + pieceCount[2] * BishopValue +
                     pieceCount[3] * RookValue + pieceCount[4] * QueenValue;
    bEval.Material = pieceCount[5] * PawnValue + pieceCount[6] * KnightValue + pieceCount[7] * BishopValue +
                     pieceCount[8] * RookValue + pieceCount[9] * QueenValue;
    return wEval.Material - bEval.Material;
}
// 2.3. Pawn structure
// 2.3.1. Doubled pawns
Middlegame int16_t doubled(const chess::Board &pos)
{
    wEval.Doubled = bEval.Doubled = 0;
    chess::Bitboard P = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE),
                    p = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);
    // We use bitwise tricks to avoid conditionals
    for (int i = 0; i < 8; ++i)
    {
        chess::Bitboard white = P & chess::attacks::MASK_FILE[i], black = p & chess::attacks::MASK_FILE[i];
        if (white.count() > 1)
            wEval.Doubled += (white.count() - 1) * Doubled;
        if (black.count() > 1)
            bEval.Doubled += (black.count() - 1) * Doubled;
    }
    return wEval.Doubled - bEval.Doubled;
}
// 2.3.2. Isolated pawns
int16_t isolated(const chess::Board &pos)
{
    chess::Bitboard P = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard p = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    constexpr chess::Bitboard noAFile = 0xFEFEFEFEFEFEFEFEULL; // mask out file A
    constexpr chess::Bitboard noHFile = 0x7F7F7F7F7F7F7F7FULL; // mask out file H

    chess::Bitboard adjP = ((P & noHFile) << 1) | ((P & noAFile) >> 1);
    chess::Bitboard adjp = ((p & noHFile) << 1) | ((p & noAFile) >> 1);

    chess::Bitboard Pi = P & ~adjP;
    chess::Bitboard pi = p & ~adjp;

    wEval.Isolated = Pi.count() * Isolated;
    bEval.Isolated = pi.count() * Isolated;

    return wEval.Isolated - bEval.Isolated;
}

// 2.3.3. Backward (https://www.chessprogramming.org/Backward_Pawns_(Bitboards))
Middlegame inline int16_t backward(const chess::Board &pos)
{
    chess::Bitboard P = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE),
                    p = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK), wstop = P << 8, bstop = p << 8,
                    wAttacks = chess::attacks::pawnLeftAttacks<chess::Color::WHITE>(P) |
                               chess::attacks::pawnRightAttacks<chess::Color::WHITE>(P),
                    bAttacks = chess::attacks::pawnLeftAttacks<chess::Color::BLACK>(p) |
                               chess::attacks::pawnRightAttacks<chess::Color::BLACK>(p),
                    wback = (wstop & bAttacks & ~wAttacks) >> 8, bback = (bstop & wAttacks & ~bAttacks) >> 8;
    wEval.Backward = wback.count() * Backward;
    bEval.Backward = bback.count() * Backward;
    return wEval.Backward - bEval.Backward;
}
// 2.3.4. Passed pawns (including candidate ones) (considered in both middlegame and endgame)

AllPhases int16_t passed(const chess::Board &pos)
{
    chess::Bitboard whitePawns = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard blackPawns = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    auto compute_score = [](chess::Bitboard P, chess::Bitboard p, bool isWhite) -> int16_t
    {
        chess::Bitboard mask = p;
        mask |= (p << 1 | p >> 1) & 0x6F6F6F6F6F6F6F6F; // ~FILE_A&~FILE_H

        if (isWhite)
        {
            mask |= (mask << 8);
            mask |= (mask << 16);
            mask |= (mask << 32);
        }
        else
        {
            mask |= (mask >> 8);
            mask |= (mask >> 16);
            mask |= (mask >> 32);
        }

        chess::Bitboard passed = P & ~mask;
        chess::Bitboard candidate = P & mask;

        return wpassed * passed.count() + cpassed * candidate.count();
    };

    wEval.Passed = compute_score(whitePawns, blackPawns, true);
    bEval.Passed = compute_score(blackPawns, whitePawns, false);

    return wEval.Passed - bEval.Passed;
}

Middlegame int16_t center(const chess::Board &pos)
{
    const chess::Square centerSquares[] = {chess::Square::SQ_E4, chess::Square::SQ_D4, chess::Square::SQ_E5,
                                           chess::Square::SQ_D5};

    int w = 0, b = 0;

    for (chess::Square sq : centerSquares)
    {
        // Count attackers
        w += chess::attacks::attackers(pos, chess::Color::WHITE, sq).count();
        b += chess::attacks::attackers(pos, chess::Color::BLACK, sq).count();
        auto p = pos.at(sq);
        if (p.color() == chess::Color::WHITE)
            ++w;
        if (p.color() == chess::Color::BLACK)
            ++b;
    }

    wEval.Center = w * Center;
    bEval.Center = b * Center;
    return wEval.Center - bEval.Center;
}
/* Pawn cache */
struct PawnEntry
{
    uint64_t key;      // Full pawn hash key (Zobrist)
    int16_t evalWhite; // Pawn eval score for White
    int16_t evalBlack; // Pawn eval score for Black
};

std::vector<PawnEntry> pawnHashTable(1 << 17);
template <bool trace>
AllPhases int pawn(const chess::Board &pos)
{
    if (trace)
    {
        return doubled(pos) + isolated(pos) + backward(pos) + center(pos);
    }
    else
    {
        // what? Nah.
        auto a = pos.hash();
        auto entry = pawnHashTable[a & 131071];
        if (entry.key == a)
            return entry.evalWhite - entry.evalBlack;
        else
        {
            int16_t score = doubled(pos) + isolated(pos) + backward(pos) + center(pos);
            PawnEntry entry = {a,
                               (int16_t)(wEval.Doubled + wEval.Isolated + wEval.Backward + wEval.Center),
                               (int16_t)(bEval.Doubled + bEval.Isolated + bEval.Backward + bEval.Center)};
            pawnHashTable[a & 131071] = entry;
            return score;
        }
    }
}
// 2.4. Mobility
AllPhases int16_t mobility(const chess::Board &board)
{
    using namespace chess;
    Bitboard occupied = board.occ();
    int mobilityScore[2] = {0, 0};
    const Color sides[2] = {Color::WHITE, Color::BLACK};

    // Lookup table for attack functions per piece type
    auto attackFuncs = [](Square sq, Bitboard occ, PieceType pt) -> Bitboard
    {
        switch (pt)
        {
        case (int)PieceType::KNIGHT:
            return attacks::knight(sq);
        case (int)PieceType::BISHOP:
            return attacks::bishop(sq, occ);
        case (int)PieceType::ROOK:
            return attacks::rook(sq, occ);
        case (int)PieceType::QUEEN:
            return attacks::queen(sq, occ);
        default:
            return 0;
        }
    };

    for (Color side : sides)
    {
        Bitboard us = board.us(side);

        for (PieceType pt : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN})
        {
            int ptIndex = static_cast<int>(pt);
            Bitboard pieces = board.pieces(pt) & us;
            while (pieces)
            {
                Square sq = pieces.pop();
                Bitboard attacks = attackFuncs(sq, occupied, pt);
                attacks &= ~us; // remove friendly squares
                mobilityScore[static_cast<int>(side)] += MOBILITY_WEIGHTS[ptIndex] * attacks.count();
            }
        }
    }

    wEval.Mobility = mobilityScore[0];
    bEval.Mobility = mobilityScore[1];
    return mobilityScore[0] - mobilityScore[1];
}

// 2.5. King safety
// 2.5.1. Pawn shield
Middlegame int16_t pawn_shield(const chess::Board &board)
{
    constexpr chess::Bitboard SHIELD = 0x00ff00000000ff00ULL;  // rank 2 and 7 for pawn shield
    constexpr chess::Bitboard SHIELD2 = 0x0000ff0000ff0000ULL; // rank 3 and 6 for isolated pawns
    chess::Bitboard wkingAttacksBB = chess::attacks::king(board.kingSq(chess::Color::WHITE)),
                    bKingAttacksBB = chess::attacks::king(board.kingSq(chess::Color::BLACK));
    int wshieldCount1 = (wkingAttacksBB & SHIELD).count(), wshieldCount2 = (wkingAttacksBB & SHIELD2).count();
    int bshieldCount1 = (bKingAttacksBB & SHIELD).count(), bshieldCount2 = (bKingAttacksBB & SHIELD2).count();
    wEval.Shield = (!wshieldCount1 * wshieldCount2 + wshieldCount1) * pawnShield;
    bEval.Shield = (!bshieldCount1 * bshieldCount2 + bshieldCount1) * pawnShield;
    return wEval.Shield - bEval.Shield;
}

Middlegame int16_t king_tropism(const chess::Board &board)
{
    using namespace chess;
    const Square wKing = board.kingSq(Color::WHITE);
    const Square bKing = board.kingSq(Color::BLACK);

    int wScore = 0, bScore = 0;

    // Consider only attacking pieces
    for (PieceType pt : {PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT})
    {
        Bitboard whiteAttackers = board.pieces(pt, Color::WHITE);
        Bitboard blackAttackers = board.pieces(pt, Color::BLACK);

        while (whiteAttackers)
        {
            Square sq = whiteAttackers.pop();
            wScore += 7 - chess::Square::value_distance(sq, bKing); // closer = higher danger
        }

        while (blackAttackers)
        {
            Square sq = blackAttackers.pop();
            bScore += 7 - chess::Square::value_distance(sq, wKing);
        }
    }

    // Scale the result
    wEval.KingTropism = wScore * KingTropism;
    bEval.KingTropism = bScore * KingTropism;

    return wEval.KingTropism - bEval.KingTropism;
}

Middlegame inline int16_t king_safety(const chess::Board &board) { return pawn_shield(board) + king_tropism(board); }
// 2.6. Space
AllPhases int space(const chess::Board &board)
{
    int spaceS[2] = {0, 0}; // [WHITE, BLACK]

    // Evaluate space control for both sides
    for (chess::Color color : {chess::Color::WHITE, chess::Color::BLACK})
    {
        chess::Color opponent = ~color;

        // Space Evaluation: Count controlled squares in the opponent's half
        chess::Bitboard my_pawns = board.pieces(chess::PieceType::PAWN, color);

        while (my_pawns)
        {
            chess::Square sq = my_pawns.pop();
            chess::Bitboard pawn_attacks = chess::attacks::pawn(color, sq);

            while (pawn_attacks)
            {
                chess::Square target = pawn_attacks.pop();
                // Check if square is in opponent's half
                bool in_opponent_half =
                    (color == chess::Color::WHITE && target >= 32) || (color == chess::Color::BLACK && target < 32);

                if (in_opponent_half && !board.isAttacked(target, opponent) && !board.at(target))
                {
                    spaceS[static_cast<int>(color)]++;
                }
            }
        }
    }
    wEval.Space = spaceS[0] * Space;
    bEval.Space = spaceS[1] * Space;
    return wEval.Space - bEval.Space;
}
// 2.7. Tempo (simple) (all phases)
AllPhases inline int16_t tempo(const chess::Board &board)
{
    return (board.sideToMove() == chess::Color::WHITE) ? Tempo : -Tempo;
}
// 2.8. Endgame
Endgame bool draw(const chess::Board &board)
{
    // KPvK with Rule of the Square
    chess::Square wk = board.kingSq(chess::Color::WHITE), bk = board.kingSq(chess::Color::BLACK);
    chess::Bitboard p = board.pieces(chess::PieceType::PAWN);
    if (!board.hasNonPawnMaterial(chess::Color::WHITE) && !board.hasNonPawnMaterial(chess::Color::BLACK) && p.count() == 1)
    {
        chess::Square pawn = p.pop();
        auto c = board.at(pawn).color();
        chess::Square promoSq(pawn.file(), chess::Rank::rank(chess::Rank::RANK_8, c));
        if (chess::Square::value_distance((c ? bk : wk), promoSq) <= chess::Square::value_distance(pawn, promoSq))
            return 1;
    }
    return board.isInsufficientMaterial();
}
template <bool Midgame>
AllPhases int16_t psqt_eval(const chess::Board &board)
{
    auto pieces = board.us(chess::Color::WHITE);

    while (pieces)
    {
        int sq = pieces.pop();
        chess::PieceType piece = board.at<chess::PieceType>(sq);
        if constexpr (Midgame)
            wEval.MGPSQT += mg_pesto_table[(int)piece][sq];
        else
            wEval.EGPSQT += eg_pesto_table[(int)piece][sq];
    }
    pieces = board.us(chess::Color::BLACK);

    while (pieces)
    {
        chess::Square sq = pieces.pop();
        chess::PieceType piece = board.at<chess::PieceType>(sq);
        sq.flip();
        if constexpr (Midgame)
            bEval.MGPSQT += mg_pesto_table[(int)piece][sq.index()];
        else
            bEval.EGPSQT += eg_pesto_table[(int)piece][sq.index()];
    }
    if constexpr (Midgame)
        return wEval.MGPSQT - bEval.MGPSQT; // Negate for Black
    else
        return wEval.EGPSQT - bEval.EGPSQT; // Negate for Black
}
// huge table for distances, idk but it gives O(1) access
static const unsigned char chebyshev_distance[64][64] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7, 4, 4, 4, 4, 4, 5, 6, 7, 5, 5, 5, 5, 5, 5, 6, 7, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7},
    {1, 0, 1, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6, 4, 4, 4, 4, 4, 4, 5, 6, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7},
    {2, 1, 0, 1, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7},
    {3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7},
    {4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7},
    {5, 4, 3, 2, 1, 0, 1, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7},
    {6, 5, 4, 3, 2, 1, 0, 1, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 4, 4, 4, 4, 4, 6, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7},
    {7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 4, 4, 4, 4, 7, 6, 5, 5, 5, 5, 5, 5, 7, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7},
    {1, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7, 4, 4, 4, 4, 4, 5, 6, 7, 5, 5, 5, 5, 5, 5, 6, 7, 6, 6, 6, 6, 6, 6, 6, 7},
    {1, 1, 1, 2, 3, 4, 5, 6, 1, 0, 1, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6, 4, 4, 4, 4, 4, 4, 5, 6, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {2, 1, 1, 1, 2, 3, 4, 5, 2, 1, 0, 1, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6},
    {3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6},
    {4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6},
    {5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 1, 0, 1, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6},
    {6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 1, 0, 1, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 4, 4, 4, 4, 4, 6, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6},
    {7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 4, 4, 4, 4, 7, 6, 5, 5, 5, 5, 5, 5, 7, 6, 6, 6, 6, 6, 6, 6},
    {2, 2, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7, 4, 4, 4, 4, 4, 5, 6, 7, 5, 5, 5, 5, 5, 5, 6, 7},
    {2, 2, 2, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 1, 0, 1, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6, 4, 4, 4, 4, 4, 4, 5, 6, 5, 5, 5, 5, 5, 5, 5, 6},
    {2, 2, 2, 2, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 1, 0, 1, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5},
    {4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5},
    {5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 1, 0, 1, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5},
    {6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 1, 0, 1, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 4, 4, 4, 4, 4, 6, 5, 5, 5, 5, 5, 5, 5},
    {7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 4, 4, 4, 4, 7, 6, 5, 5, 5, 5, 5, 5},
    {3, 3, 3, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7, 4, 4, 4, 4, 4, 5, 6, 7},
    {3, 3, 3, 3, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 1, 0, 1, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6, 4, 4, 4, 4, 4, 4, 5, 6},
    {3, 3, 3, 3, 3, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 1, 0, 1, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 5},
    {3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4},
    {4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4},
    {5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 1, 0, 1, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 4, 4, 4, 4, 4, 4},
    {6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 1, 0, 1, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 4, 4, 4, 4, 4},
    {7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 4, 4, 4, 4},
    {4, 4, 4, 4, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7},
    {4, 4, 4, 4, 4, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 1, 0, 1, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6},
    {4, 4, 4, 4, 4, 4, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 1, 0, 1, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5},
    {4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3, 4},
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 3, 3, 3, 3, 3, 3},
    {5, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 1, 0, 1, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 3, 3, 3, 3, 3},
    {6, 5, 4, 4, 4, 4, 4, 4, 6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 1, 0, 1, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 3, 3, 3, 3},
    {7, 6, 5, 4, 4, 4, 4, 4, 7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 3, 3, 3},
    {5, 5, 5, 5, 5, 5, 6, 7, 4, 4, 4, 4, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7},
    {5, 5, 5, 5, 5, 5, 5, 6, 4, 4, 4, 4, 4, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 1, 0, 1, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6},
    {5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 1, 0, 1, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5},
    {5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4},
    {5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 2, 2, 2, 2, 3},
    {5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 1, 0, 1, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 2, 2, 2, 2},
    {6, 5, 5, 5, 5, 5, 5, 5, 6, 5, 4, 4, 4, 4, 4, 4, 6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 1, 0, 1, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 2, 2, 2},
    {7, 6, 5, 5, 5, 5, 5, 5, 7, 6, 5, 4, 4, 4, 4, 4, 7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 2, 2},
    {6, 6, 6, 6, 6, 6, 6, 7, 5, 5, 5, 5, 5, 5, 6, 7, 4, 4, 4, 4, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7},
    {6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 6, 4, 4, 4, 4, 4, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 1, 0, 1, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6},
    {6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 1, 0, 1, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5},
    {6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4},
    {6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3},
    {6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 1, 0, 1, 2, 5, 4, 3, 2, 1, 1, 1, 2},
    {6, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 6, 5, 4, 4, 4, 4, 4, 4, 6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 1, 0, 1, 6, 5, 4, 3, 2, 1, 1, 1},
    {7, 6, 6, 6, 6, 6, 6, 6, 7, 6, 5, 5, 5, 5, 5, 5, 7, 6, 5, 4, 4, 4, 4, 4, 7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 1},
    {7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 7, 5, 5, 5, 5, 5, 5, 6, 7, 4, 4, 4, 4, 4, 5, 6, 7, 3, 3, 3, 3, 4, 5, 6, 7, 2, 2, 2, 3, 4, 5, 6, 7, 1, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7},
    {7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 6, 4, 4, 4, 4, 4, 4, 5, 6, 3, 3, 3, 3, 3, 4, 5, 6, 2, 2, 2, 2, 3, 4, 5, 6, 1, 1, 1, 2, 3, 4, 5, 6, 1, 0, 1, 2, 3, 4, 5, 6},
    {7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 5, 3, 3, 3, 3, 3, 3, 4, 5, 2, 2, 2, 2, 2, 3, 4, 5, 2, 1, 1, 1, 2, 3, 4, 5, 2, 1, 0, 1, 2, 3, 4, 5},
    {7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4},
    {7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 3, 4, 3, 2, 1, 1, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3},
    {7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 3, 3, 3, 3, 3, 5, 4, 3, 2, 2, 2, 2, 2, 5, 4, 3, 2, 1, 1, 1, 2, 5, 4, 3, 2, 1, 0, 1, 2},
    {7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 6, 5, 4, 4, 4, 4, 4, 4, 6, 5, 4, 3, 3, 3, 3, 3, 6, 5, 4, 3, 2, 2, 2, 2, 6, 5, 4, 3, 2, 1, 1, 1, 6, 5, 4, 3, 2, 1, 0, 1},
    {7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 7, 6, 5, 5, 5, 5, 5, 5, 7, 6, 5, 4, 4, 4, 4, 4, 7, 6, 5, 4, 3, 3, 3, 3, 7, 6, 5, 4, 3, 2, 2, 2, 7, 6, 5, 4, 3, 2, 1, 1, 7, 6, 5, 4, 3, 2, 1, 0}
};
inline int distance_to_corner(uint8_t sq){
	return std::min({chebyshev_distance[sq][0],   // A1
					 chebyshev_distance[sq][56],  // A8
					 chebyshev_distance[sq][7],   // H1
					 chebyshev_distance[sq][63]});// H8
}
inline int distance_from_center(uint8_t sq){
	return std::min({chebyshev_distance[sq][34],  // E4
					 chebyshev_distance[sq][35],  // D4
					 chebyshev_distance[sq][42],  // E5
					 chebyshev_distance[sq][43]});// D5
}
int mopUp(const chess::Board& pos) {
    chess::Square whiteKingSq = pos.kingSq(chess::Color::WHITE);
    chess::Square blackKingSq = pos.kingSq(chess::Color::BLACK);

    int whiteMaterial = wEval.Material;
    int blackMaterial = bEval.Material;
    int materialAdvantage = whiteMaterial - blackMaterial;

    // If neither side has a big material advantage, skip mop-up
    if (std::abs(materialAdvantage) < 300) return 0;

    if (wEval.phase>64) return 0;

    // Distance to corner for both kings
    int whiteCornerDist = distance_to_corner(whiteKingSq.index());
    int blackCornerDist = distance_to_corner(blackKingSq.index());

    // Distance from center for both kings
    int whiteCenterDist = distance_from_center(whiteKingSq.index());
    int blackCenterDist = distance_from_center(blackKingSq.index());

    // White mop-up score (push black king to corner, bring white king to center)
    int whiteMopUp = blackCornerDist * 4 - whiteCenterDist * 3;

    // Black mop-up score (push white king to corner, bring black king to center)
    int blackMopUp = whiteCornerDist * 4 - blackCenterDist * 3;

    // Return net mop-up score favoring White (+ means White advantage)
    return whiteMopUp - blackMopUp;
}

// 3. Main evaluation
inline int16_t eg(const chess::Board &pos)
{
    if (draw(pos))
        return 0;
    return space(pos) + tempo(pos) + material(pos) + passed(pos) + psqt_eval<false>(pos) + mobility(pos) + king_safety(pos) + mopUp(pos);
}
template <bool trace = false>
inline int16_t mg(const chess::Board &pos)
{
    return material(pos) + space(pos) + tempo(pos) + mobility(pos) + king_safety(pos) + psqt_eval<true>(pos) + pawn<trace>(pos);
}
struct CachedEvalEntry
{
    uint64_t key; // Full pawn hash key (Zobrist)
    int16_t eval;
};

std::vector<CachedEvalEntry> evalCache(1 << 17);
template <bool trace = false>
int16_t eval(const chess::Board &pos)
{
    auto hash = pos.hash();
    auto &entry = evalCache[hash & 131071];
    if constexpr (!trace)
        if (entry.key == hash)
            return entry.eval;
    memset(&wEval, 0, sizeof(wEval));
    memset(&bEval, 0, sizeof(bEval));
    const int phase = ::phase(pos);
    const int sign = pos.sideToMove() == chess::Color::WHITE ? 1 : -1;

    int mgScore = mg<trace>(pos);
    int egScore = eg(pos);
    int finalScore = ((mgScore * phase) + (egScore * (256 - phase))) / 256 * sign;

    return finalScore;
}
int16_t eval(const chess::Board &pos)
{
    return eval<true>(pos);
}
void traceEvaluationResults(const char *label = nullptr, int16_t _eval = 0)
{
    if (label)
        printf("\n[Trace: %s]\n", label);

    printf("+----------------+-------+-------+\n");
    printf("| Factor         | White | Black |\n");
    printf("+----------------+-------+-------+\n");
    printf("| Phase          | %13d |\n", wEval.phase);
    printf("| Material       | %5d | %5d |\n", wEval.Material, bEval.Material);
    printf("| Doubled        | %5d | %5d |\n", wEval.Doubled, bEval.Doubled);
    printf("| Isolated       | %5d | %5d |\n", wEval.Isolated, bEval.Isolated);
    printf("| Backward       | %5d | %5d |\n", wEval.Backward, bEval.Backward);
    printf("| Passed         | %5d | %5d |\n", wEval.Passed, bEval.Passed);
    printf("| King tropism   | %5d | %5d |\n", wEval.KingTropism, bEval.KingTropism);
    printf("| Center control | %5d | %5d |\n", wEval.Center, bEval.Center);
    printf("| Mobility       | %5d | %5d |\n", wEval.Mobility, bEval.Mobility);
    printf("| Shield         | %5d | %5d |\n", wEval.Shield, bEval.Shield);
    printf("| Space          | %5d | %5d |\n", wEval.Space, bEval.Space);
    printf("| MiddlegamePSQT | %5d | %5d |\n", wEval.MGPSQT, bEval.MGPSQT);
    printf("| EndgamePSQT    | %5d | %5d |\n", wEval.EGPSQT, bEval.EGPSQT);
    printf("| Total          | %13d |\n", _eval);
    printf("+----------------+-------+-------+\n");
}
void trace(const chess::Board &pos) { traceEvaluationResults("Handcrafted", eval<true>(pos)); }
int16_t piece_value(chess::PieceType p)
{
    switch ((int)p)
    {
    case (int)chess::PieceType::PAWN:
        return PawnValue;
    case (int)chess::PieceType::KNIGHT:
        return KnightValue;
    case (int)chess::PieceType::BISHOP:
        return BishopValue;
    case (int)chess::PieceType::ROOK:
        return RookValue;
    case (int)chess::PieceType::QUEEN:
        return QueenValue;
    default:
        return 0;
    }
}
