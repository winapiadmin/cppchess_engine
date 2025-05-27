#include "eval.h"
#define Evaluation [[nodiscard]]
#define Middlegame Evaluation
#define Endgame Evaluation
#define AllPhases Middlegame Endgame
#define EvaluationResult int16_t
// 0. Evaluation result

struct EvalBreakdown {
    EvaluationResult Material;
    EvaluationResult Doubled;
    EvaluationResult Isolated;
    EvaluationResult Backward;
    EvaluationResult Passed;
    EvaluationResult Islands;
    EvaluationResult Holes;
    EvaluationResult Storm;
    EvaluationResult Center;
    EvaluationResult Mobility;
    EvaluationResult Shield;
    EvaluationResult King;
    EvaluationResult Space;
    EvaluationResult PSQT;
};

EvalBreakdown wEval{}, bEval{};
// 1. Weights, in case of tuning (centipawns)
// 1.1. Material weights
constexpr int16_t PawnValue = 100,
                  KnightValue = 325,
                  BishopValue = 350,
                  RookValue = 500,
                  QueenValue = 900;
// 1.2. Pawn structure
constexpr int16_t Doubled = -100,
                  Isolated = -80,
                  Backward = -100,
                  wpassed = 100,
                  cpassed = 50,
                  Island = -30,
                  Center = 100,
                  hole = -50;
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
constexpr int16_t pawnShield = 20,
                  pawnStorm = 12;
// 1.4. Space
constexpr int16_t Space = 10;
// 1.5. Tempo
constexpr int16_t Tempo = 28;
// 1.6. PQST (source: Chessprogramming Wiki)
// PAWN
const int PAWN_PSQT[2][64] = {
    {// Opening
      0,   0,   0,   0,   0,   0,   0,   0,
     50,  50,  50,  50,  50,  50,  50,  50,
     10,  10,  20,  30,  30,  20,  10,  10,
      5,   5,  10,  25,  25,  10,   5,   5,
      0,   0,   0,  20,  20,   0,   0,   0,
      5,  -5, -10,   0,   0, -10,  -5,   5,
      5,  10,  10, -20, -20,  10,  10,   5,
      0,   0,   0,   0,   0,   0,   0,   0},
    {// Endgame
      0,   0,   0,   0,   0,   0,   0,   0,
     50,  50,  50,  50,  50,  50,  50,  50,
     10,  10,  20,  30,  30,  20,  10,  10,
      5,   5,  10,  25,  25,  10,   5,   5,
      0,   0,   0,  20,  20,   0,   0,   0,
      5,  -5, -10,   0,   0, -10,  -5,   5,
      5,  10,  10, -20, -20,  10,  10,   5,
      0,   0,   0,   0,   0,   0,   0,   0}
};

// KNIGHT
const int KNIGHT_PSQT[2][64] = {
    {// Opening
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50},
    {// Endgame
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50}
};

// BISHOP
const int BISHOP_PSQT[2][64] = {
    {// Opening
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20},
    {// Endgame
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20}
};

// ROOK
const int ROOK_PSQT[2][64] = {
    {// Opening
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
     25,  25,  25,  25,  25,  25,  25,  25,
      0,   0,   0,   5,   5,   0,   0,   0},
    {// Endgame
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
      0,   0,   5,  10,  10,   5,   0,   0,
     25,  25,  25,  25,  25,  25,  25,  25,
      0,   0,   0,   5,   5,   0,   0,   0}
};

// QUEEN
const int QUEEN_PSQT[2][64] = {
    {// Opening
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   5,   0,   0,   5,   0, -10,
    -10,   5,   5,   5,   5,   5,   5, -10,
     -5,   0,   5,   5,   5,   5,   0,  -5,
     -5,   0,   5,   5,   5,   5,   0,  -5,
    -10,   5,   5,   5,   5,   5,   5, -10,
    -10,   0,   5,   0,   0,   5,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20},
    {// Endgame
    -10,  -5,  -5,   0,   0,  -5,  -5, -10,
     -5,   0,   0,   5,   5,   0,   0,  -5,
     -5,   0,   5,   5,   5,   5,   0,  -5,
      0,   5,   5,   5,   5,   5,   5,   0,
      0,   5,   5,   5,   5,   5,   5,   0,
     -5,   0,   5,   5,   5,   5,   0,  -5,
     -5,   0,   0,   5,   5,   0,   0,  -5,
    -10,  -5,  -5,   0,   0,  -5,  -5, -10}
};

// KING
const int KING_PSQT[2][64] = {
    {// Opening
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
     20,  20,   0,   0,   0,   0,  20,  20,
     20,  30,  10, -30,   0,  10,  30,  20},
    {// Endgame
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10,   0,   0, -10, -20, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -30,   0,   0,   0,   0, -30, -30,
    -50, -30, -30, -30, -30, -30, -30, -50}
};

// 2. Evaluation functions
// 2.1. Phase
Evaluation double phase(const chess::Position &pos)
{
    constexpr int KnightPhase = 1;
    constexpr int BishopPhase = 1;
    constexpr int RookPhase = 2;
    constexpr int QueenPhase = 4;
    constexpr int TotalPhase = KnightPhase * 4 + BishopPhase * 4 + RookPhase * 4 + QueenPhase * 2;

    int phase =
        (pos.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).count() +
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

    return double(phase * 256 + TotalPhase / 2) / TotalPhase;
}
// 2.2. Material (one-line) (all phases)
AllPhases inline int16_t material(const chess::Position &pos)
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
    wEval.Material= pieceCount[0] * PawnValue + pieceCount[1] * KnightValue + pieceCount[2] * BishopValue + pieceCount[3] * RookValue + pieceCount[4] * QueenValue;
    bEval.Material = pieceCount[5] * PawnValue + pieceCount[6] * KnightValue + pieceCount[7] * BishopValue + pieceCount[8] * RookValue + pieceCount[9] * QueenValue;
    return wEval.Material - bEval.Material;
}
// 2.3. Pawn structure
// 2.3.1. Doubled pawns
Middlegame int16_t doubled(const chess::Position &pos)
{
    wEval.Doubled=bEval.Doubled=0;
    chess::Bitboard P = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE),
                    p = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);
    // We use bitwise tricks to avoid conditionals
    for (int i = 0; i < 8; ++i)
    {
        chess::Bitboard white = P & chess::attacks::MASK_FILE[i],
                        black = p & chess::attacks::MASK_FILE[i];
        int count = white.count() - black.count();
        if (count > 1)
        {
            wEval.Doubled += (white.count()-1) * Doubled;
            bEval.Doubled += (black.count()-1) * Doubled;
        }
    }
    return wEval.Doubled - bEval.Doubled;
}
// 2.3.2. Isolated pawns
Middlegame int16_t isolated(const chess::Position &pos)
{
    chess::Bitboard P = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE),
                    p = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK),
                    adjP = (P << 1 | P >> 1) & 0x6F6F6F6F6F6F6F6F,
                    adjp = (p << 1 | p >> 1) & 0x6F6F6F6F6F6F6F6F,
                    Pi = P & ~adjP,
                    pi = p & ~adjp;
    wEval.Isolated = Pi.count() * Isolated;
    bEval.Isolated = pi.count() * Isolated;
    return wEval.Isolated - bEval.Isolated;
}
// 2.3.3. Backward (https://www.chessprogramming.org/Backward_Pawns_(Bitboards))
Middlegame inline int16_t backward(const chess::Position &pos)
{
    chess::Bitboard P = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE),
                    p = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK),
                    wstop = P << 8,
                    bstop = p << 8,
                    wAttacks = chess::attacks::pawnLeftAttacks<chess::Color::WHITE>(P) |
                               chess::attacks::pawnRightAttacks<chess::Color::WHITE>(P),
                    bAttacks = chess::attacks::pawnLeftAttacks<chess::Color::BLACK>(p) |
                               chess::attacks::pawnRightAttacks<chess::Color::BLACK>(p),
                    wback = (wstop & bAttacks & ~wAttacks) >> 8,
                    bback = (bstop & wAttacks & ~bAttacks) >> 8;
    wEval.Backward = wback.count()*Backward;
    bEval.Backward = bback.count()*Backward;
    return wEval.Backward-bEval.Backward;
}
// 2.3.4. Passed pawns (including candidate ones) (considered in both middlegame and endgame)

AllPhases int16_t passed(const chess::Position &pos)
{
    chess::Bitboard whitePawns = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard blackPawns = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    auto compute_score = [](chess::Bitboard P, chess::Bitboard p, bool isWhite) -> int16_t
    {
        chess::Bitboard mask = p;
        mask |= (p << 1 | p >> 1) & 0x6F6F6F6F6F6F6F6F;// ~FILE_A&~FILE_H

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

// 2.3.5. Pawn islands
Middlegame int16_t islands(const chess::Position &pos)
{
    auto countIslands = [](chess::Bitboard pawns) -> int
    {
        int islands = 0;
        while (pawns)
        {
            // Pop one pawn bit
            chess::Bitboard bit = pawns.pop();

            // Flood fill horizontally connected pawns in adjacent files
            chess::Bitboard connected = bit;
            chess::Bitboard prev;
            do
            {
                prev = connected;
                connected |= (connected << 1) & pawns; // left adjacent files
                connected |= (connected >> 1) & pawns; // right adjacent files
                pawns &= ~connected;                   // remove connected from pawns
            } while (connected != prev);

            islands++;
        }
        return islands;
    };

    chess::Bitboard whitePawns = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard blackPawns = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    wEval.Islands = countIslands(whitePawns) * Island;
    bEval.Islands = countIslands(blackPawns) * Island;

    return wEval.Islands - bEval.Islands;
}

// 2.3.6. Holes
Middlegame int16_t holes(const chess::Position &pos)
{
    constexpr chess::Bitboard FILE_A_MASK = ~chess::attacks::MASK_FILE[0];
    constexpr chess::Bitboard FILE_H_MASK = ~chess::attacks::MASK_FILE[7];

    chess::Bitboard whitePawns = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard blackPawns = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    // Defensive coverage for white
    chess::Bitboard whiteDef = ((whitePawns << 8) | (whitePawns >> 8)) |
                               ((whitePawns << 1) & FILE_A_MASK) |
                               ((whitePawns >> 1) & FILE_H_MASK);

    // Defensive coverage for black
    chess::Bitboard blackDef = ((blackPawns << 8) | (blackPawns >> 8)) |
                               ((blackPawns << 1) & FILE_A_MASK) |
                               ((blackPawns >> 1) & FILE_H_MASK);

    // Mask out actual pawn presence
    chess::Bitboard whiteHoles = ~(whitePawns | whiteDef);
    chess::Bitboard blackHoles = ~(blackPawns | blackDef);

    // Restrict to middle game ranks (ranks 3–6) where holes matter most
    constexpr chess::Bitboard MID_RANKS = 0x00FFFF0000ULL; // Ranks 3–6
    whiteHoles &= MID_RANKS;
    blackHoles &= MID_RANKS;

    // Apply penalty for each hole
    wEval.Holes = whiteHoles.count() * hole;
    bEval.Holes = blackHoles.count() * hole;

    return wEval.Holes - bEval.Holes;
}
int16_t center(const chess::Position& pos) {
    const chess::Square centerSquares[] = {chess::Square::SQ_E4, chess::Square::SQ_D4, chess::Square::SQ_E5, chess::Square::SQ_D5};

    int w = 0, b = 0;

    for (chess::Square sq : centerSquares) {
        // Count attackers
        w += chess::attacks::attackers(pos, chess::Color::WHITE, sq).count();
        b += chess::attacks::attackers(pos, chess::Color::BLACK, sq).count();
        auto p=pos.at(sq);
        if (p.color()==chess::Color::WHITE)++w;
        if (p.color()==chess::Color::BLACK)++b;
    }

    wEval.Center = w * Center;
    bEval.Center = b * Center;
    return wEval.Center - bEval.Center;
}


Middlegame int pawn(const chess::Position &pos)
{
    return doubled(pos)+isolated(pos)+backward(pos)+islands(pos)+holes(pos)+center(pos);
}
// 2.4. Mobility
AllPhases int16_t mobility(const chess::Position &board)
{
    using namespace chess;
    Bitboard occupied = board.occ();
    int mobilityScore[2] = {0, 0};
    const Color sides[2] = {Color::WHITE, Color::BLACK};

    // Lookup table for attack functions per piece type
    auto attackFuncs = [](Square sq, Bitboard occ, PieceType pt) -> Bitboard {
        switch (pt)
        {
            case (int)PieceType::KNIGHT: return attacks::knight(sq);
            case (int)PieceType::BISHOP: return attacks::bishop(sq, occ);
            case (int)PieceType::ROOK:   return attacks::rook(sq, occ);
            case (int)PieceType::QUEEN:  return attacks::queen(sq, occ);
            default: return 0;
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
Middlegame int16_t pawn_shield(const chess::Position &board)
{
    constexpr chess::Bitboard SHIELD = 0x00ff00000000ff00ULL;  // rank 2 and 7 for pawn shield
    constexpr chess::Bitboard SHIELD2 = 0x0000ff0000ff0000ULL; // rank 3 and 6 for isolated pawns
    chess::Bitboard wkingAttacksBB = chess::attacks::king(board.kingSq(chess::Color::WHITE)),
                    bKingAttacksBB = chess::attacks::king(board.kingSq(chess::Color::BLACK));
    int wshieldCount1 = (wkingAttacksBB & SHIELD).count(),
        wshieldCount2 = (wkingAttacksBB & SHIELD2).count();
    int bshieldCount1 = (bKingAttacksBB & SHIELD).count(),
        bshieldCount2 = (bKingAttacksBB & SHIELD2).count();
    wEval.Shield = (!wshieldCount1 * wshieldCount2 + wshieldCount1) * pawnShield,
    bEval.Shield = (!bshieldCount1 * bshieldCount2 + bshieldCount1) * pawnShield;
    return wEval.Shield - bEval.Shield;
}
// 2.5.2. Pawn storm (simple)
// Only consider opponent's pawn near the king
Middlegame int16_t pawn_storm(const chess::Position &board)
{
    chess::Square whiteKing = board.kingSq(chess::Color::WHITE);
    chess::Square blackKing = board.kingSq(chess::Color::BLACK);

    chess::Bitboard whitePawns = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard blackPawns = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    constexpr chess::Bitboard KINGSIDE_FLANK = chess::attacks::MASK_FILE[4] | chess::attacks::MASK_FILE[5] | chess::attacks::MASK_FILE[6] | chess::attacks::MASK_FILE[7];
    constexpr chess::Bitboard QUEENSIDE_FLANK = chess::attacks::MASK_FILE[0] | chess::attacks::MASK_FILE[1] | chess::attacks::MASK_FILE[2] | chess::attacks::MASK_FILE[3];
    constexpr chess::Bitboard stormZone = chess::attacks::MASK_RANK[3] | chess::attacks::MASK_RANK[4];

    chess::Bitboard whiteFlankMask = (whiteKing.file() >= chess::File::FILE_E) ? KINGSIDE_FLANK : QUEENSIDE_FLANK;
    chess::Bitboard blackFlankMask = (blackKing.file() >= chess::File::FILE_E) ? KINGSIDE_FLANK : QUEENSIDE_FLANK;

    wEval.Storm = (blackPawns & stormZone & whiteFlankMask).count() * pawnStorm;
    bEval.Storm = (whitePawns & stormZone & blackFlankMask).count() * pawnStorm;
    // Positive score if Black storms White, negative if White storms Black (from White POV)
    return wEval.Storm - bEval.Storm;
}
Middlegame int16_t king_safety(const chess::Position &board)
{
    return pawn_shield(board) + pawn_storm(board);
}
// 2.6. Space
AllPhases int space(const chess::Position &board)
{
    int score = 0;

    int spaceS[2] = {0, 0}; // [WHITE, BLACK]

    // Evaluate space control for both sides
    for (chess::Color color : {chess::Color::WHITE, chess::Color::BLACK})
    {
        chess::Color opponent = ~color;
        int color_multiplier = (color == chess::Color::WHITE) ? 1 : -1;

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
                bool in_opponent_half = (color == chess::Color::WHITE && target >= 32) ||
                                        (color == chess::Color::BLACK && target < 32);

                if (in_opponent_half && !board.isAttacked(target, opponent) && !board.at(target))
                {
                    spaceS[static_cast<int>(color)]++;
                    score += Space * color_multiplier;
                }
            }
        }
    }
    wEval.Space = spaceS[0];
    bEval.Space = spaceS[1];
    return score;
}
// 2.7. Tempo (simple) (all phases)
AllPhases int16_t tempo(const chess::Position &board)
{
    return (board.sideToMove() == chess::Color::WHITE) ? Tempo : -Tempo;
}
// 2.8. Endgame
Endgame bool draw(const chess::Position &board)
{
    // Precompute piece counts (avoid multiple function calls)
    int pieceCount[10] = {
        board.pieces(chess::PieceType::PAWN, chess::Color::WHITE).count(),
        board.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).count(),
        board.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).count(),
        board.pieces(chess::PieceType::ROOK, chess::Color::WHITE).count(),
        board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE).count(),
        board.pieces(chess::PieceType::PAWN, chess::Color::BLACK).count(),
        board.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK).count(),
        board.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).count(),
        board.pieces(chess::PieceType::ROOK, chess::Color::BLACK).count(),
        board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK).count(),
    };

    // Count total non-pawn pieces
    int totalNonPawnPieces = 0;
    for (int i = 1; i < 5; ++i)
    {
        totalNonPawnPieces += pieceCount[i] + pieceCount[i + 5];
    }

    // Total pawns
    int totalPawns = pieceCount[0] + pieceCount[5];

    // KvK
    if (totalNonPawnPieces == 0 && totalPawns == 0)
        return 0;

    // KBvK or KNvK
    if (totalNonPawnPieces == 1 && totalPawns == 0 &&
        (pieceCount[1] == 1 || pieceCount[2] == 1 ||
         pieceCount[6] == 1 || pieceCount[7] == 1))
        return 1;

    // KBBvK with same colored bishops
    if (totalNonPawnPieces == 2 && totalPawns == 0)
    {
        // Check white bishops on same color
        if (pieceCount[2] == 2)
        {
            chess::Bitboard bishops = board.pieces(chess::PieceType::BISHOP, chess::Color::WHITE);
            chess::Square first = bishops.pop();
            chess::Square second = bishops.pop();
            if (chess::Square::same_color(first, second))
                return 1;
        }
        // Check black bishops on same color
        else if (pieceCount[7] == 2)
        {
            chess::Bitboard bishops = board.pieces(chess::PieceType::BISHOP, chess::Color::BLACK);
            chess::Square first = bishops.pop();
            chess::Square second = bishops.pop();
            if (chess::Square::same_color(first, second))
                return 1;
        }

        // KNNvK
        if ((pieceCount[1] == 2 && pieceCount[6] == 0) ||
            (pieceCount[1] == 0 && pieceCount[6] == 2))
            return 1;

        // KBNvK
        if ((pieceCount[1] == 1 && pieceCount[2] == 1) ||
            (pieceCount[6] == 1 && pieceCount[7] == 1))
            return 1;

        // KNvKN
        if (pieceCount[1] == 1 && pieceCount[6] == 1)
            return 1;
    }
    // KPvK with Rule of the Square
    chess::Square wk = board.kingSq(chess::Color::WHITE),
                  bk = board.kingSq(chess::Color::BLACK);
    if (totalNonPawnPieces == 0 && totalPawns == 1)
    {
        chess::Square pawn = board.pieces(chess::PieceType::PAWN).pop();
        auto c = board.at(pawn).color();
        chess::Square promoSq(pawn.file(), chess::Rank::rank(chess::Rank::RANK_8, c));
        if (chess::Square::value_distance((c ? bk : wk), promoSq) > chess::Square::value_distance(pawn, promoSq))
            return 1;
    }
    return 0;
}

/**
 * Evaluates the Piece-Square Table (PSQT) score for both white and black pieces.
 * 
 * Calculates the positional value of pieces based on their location on the board,
 * taking into account the game phase (opening or endgame). The function iterates 
 * through white and black pieces, adding their positional scores to respective 
 * global PSQT variables.
 * 
 * @param board The current chess board position
 * @return int The total PSQT score (though currently always returns 0)
 */
Evaluation int16_t psqt_eval(const chess::Position &board)
{
    int score = 0;
    int phaseIndex = (phase(board) >=0.7) ? 1 : 0; // 0 = Opening, 1 = Endgame

    auto pieces = board.us(chess::Color::WHITE);

    while (pieces)
    {
        int sq = pieces.pop();
        chess::Piece piece = board.at(sq);
        switch (piece.type())
        {
        case (int)chess::PieceType::underlying::PAWN:
            wEval.PSQT += PAWN_PSQT[phaseIndex][sq];
            break;
        case (int)chess::PieceType::underlying::KNIGHT:
            wEval.PSQT += KNIGHT_PSQT[phaseIndex][sq];
            break;
        case (int)chess::PieceType::underlying::BISHOP:
            wEval.PSQT += BISHOP_PSQT[phaseIndex][sq];
            break;
        case (int)chess::PieceType::underlying::ROOK:
            wEval.PSQT += ROOK_PSQT[phaseIndex][sq];
            break;
        case (int)chess::PieceType::underlying::QUEEN:
            wEval.PSQT += QUEEN_PSQT[phaseIndex][sq];
            break;
        case (int)chess::PieceType::underlying::KING:
            wEval.PSQT += KING_PSQT[phaseIndex][sq];
            break;
        default:
            break;
        }
    }
    pieces = board.us(chess::Color::BLACK);

    while (pieces)
    {
        chess::Square sq = pieces.pop();
        chess::Piece piece = board.at(sq);
        sq.flip();
        switch (piece.type())
        {
        case (int)chess::PieceType::underlying::PAWN:
            bEval.PSQT += PAWN_PSQT[phaseIndex][sq.index()];
            break;
        case (int)chess::PieceType::underlying::KNIGHT:
            bEval.PSQT += KNIGHT_PSQT[phaseIndex][sq.index()];
            break;
        case (int)chess::PieceType::underlying::BISHOP:
            bEval.PSQT += BISHOP_PSQT[phaseIndex][sq.index()];
            break;
        case (int)chess::PieceType::underlying::ROOK:
            bEval.PSQT += ROOK_PSQT[phaseIndex][sq.index()];
            break;
        case (int)chess::PieceType::underlying::QUEEN:
            bEval.PSQT += QUEEN_PSQT[phaseIndex][sq.index()];
            break;
        case (int)chess::PieceType::underlying::KING:
            bEval.PSQT += KING_PSQT[phaseIndex][sq.index()];
            break;
        default:
            break;
        }
    }
    return score; // Negate for Black
}
// 3. Main evaluation
int16_t eg(const chess::Position &pos)
{
    if (draw(pos))
        return 0;
    int score = space(pos) + tempo(pos) + material(pos) + passed(pos);
    return score;
}
int16_t mg(const chess::Position &pos)
{
    return material(pos) + space(pos) + tempo(pos) + mobility(pos) + king_safety(pos);
}
int16_t eval(const chess::Position &pos)
{
    const int phase = ::phase(pos);
    const int sign = pos.sideToMove() == chess::Color::WHITE ? 1 : -1;

    int mgScore = mg(pos);
    int egScore = eg(pos);
    int finalScore = ((mgScore * phase) + (egScore * (256 - phase))) / 256 * sign;

    return finalScore;
}

void traceEvaluationResults(const char* label = nullptr, int16_t _eval=0) {
    if (label) printf("\n[Trace: %s]\n", label);

    printf("+----------------+-------+-------+\n");
    printf("| Factor         | White | Black |\n");
    printf("+----------------+-------+-------+\n");
    printf("| Material       | %5d | %5d |\n", wEval.Material, bEval.Material);
    printf("| Doubled        | %5d | %5d |\n", wEval.Doubled, bEval.Doubled);
    printf("| Isolated       | %5d | %5d |\n", wEval.Isolated, bEval.Isolated);
    printf("| Backward       | %5d | %5d |\n", wEval.Backward, bEval.Backward);
    printf("| Passed         | %5d | %5d |\n", wEval.Passed, bEval.Passed);
    printf("| Islands        | %5d | %5d |\n", wEval.Islands, bEval.Islands);
    printf("| Holes          | %5d | %5d |\n", wEval.Holes, bEval.Holes);
    printf("| Storm          | %5d | %5d |\n", wEval.Storm, bEval.Storm);
    printf("| Center control | %5d | %5d |\n", wEval.Center, bEval.Center);
    printf("| Mobility       | %5d | %5d |\n", wEval.Mobility, bEval.Mobility);
    printf("| Shield         | %5d | %5d |\n", wEval.Shield, bEval.Shield);
    printf("| Space          | %5d | %5d |\n", wEval.Space, bEval.Space);
    printf("| PSQT           | %5d | %5d |\n", wEval.PSQT, bEval.PSQT);
    printf("| Total          | %13d |\n", _eval);
    printf("+----------------+-------+-------+\n");
}
void trace(const chess::Position& pos){
    traceEvaluationResults("Handcrafted", eval(pos));
}
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
