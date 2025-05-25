#include "eval.h"
#define Evaluation [[nodiscard]]
#define Middlegame Evaluation
#define Endgame Evaluation
#define AllPhases Middlegame Endgame
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
                  pawnStorm = 12,
                  kingNearFile = 30;
// 1.4. Space
constexpr int16_t Space = 10;
// 1.5. Tempo
constexpr int16_t Tempo = 28;
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
{
    return (pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE).count() -
            pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK).count()) *
               PawnValue +
           (pos.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).count() -
            pos.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK).count()) *
               KnightValue +
           (pos.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).count() -
            pos.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).count()) *
               BishopValue +
           (pos.pieces(chess::PieceType::ROOK, chess::Color::WHITE).count() -
            pos.pieces(chess::PieceType::ROOK, chess::Color::BLACK).count()) *
               RookValue +
           (pos.pieces(chess::PieceType::QUEEN, chess::Color::WHITE).count() -
            pos.pieces(chess::PieceType::QUEEN, chess::Color::BLACK).count()) *
               QueenValue;
}
// 2.3. Pawn structure
// 2.3.1. Doubled pawns
Middlegame int16_t doubled(const chess::Position &pos)
{
    int16_t score = 0;
    chess::Bitboard P = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE),
                    p = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);
    // We use bitwise tricks to avoid conditionals
    for (int i = 0; i < 8; ++i)
    {
        chess::Bitboard white = P & chess::attacks::MASK_FILE[i],
                        black = p & chess::attacks::MASK_FILE[i];
        int count = white.count() - black.count();
        if (count > 1)
            score += count;
    }
    return score * Doubled;
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
    return (Pi.count() - pi.count()) * Isolated;
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
    return -(wback.count() - bback.count()) * Backward;
}
// 2.3.4. Passed pawns (including candidate ones) (considered in both middlegame and endgame)

AllPhases int16_t passed(const chess::Position &pos)
{
    chess::Bitboard whitePawns = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard blackPawns = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    auto compute_score = [](chess::Bitboard P, chess::Bitboard p, bool isWhite) -> int16_t
    {
        chess::Bitboard mask = p;
        mask |= (p << 1 | p >> 1) & 0x6F6F6F6F6F6F6F6F;

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

    int16_t whiteScore = compute_score(whitePawns, blackPawns, true);
    int16_t blackScore = compute_score(blackPawns, whitePawns, false);

    return whiteScore - blackScore;
}


// 2.3.5. Pawn islands
// Function to evaluate the difference in pawn islands between white and black
Middlegame int16_t islands(const chess::Position &pos)
{
    constexpr chess::Bitboard NFILE_A = ~chess::attacks::MASK_FILE[0],
                              NRANK_8 = ~chess::attacks::MASK_RANK[7],
                              NFILE_H = ~chess::attacks::MASK_FILE[7],
                              NRANK_1 = ~chess::attacks::MASK_RANK[0];
    // Get the pawns of both white and black
    chess::Bitboard whitePawns = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard blackPawns = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    int16_t whitePenalty = 0;
    int16_t blackPenalty = 0;

    chess::Bitboard visitedWhite = 0; // Bitboard to track white pawns we've already processed
    chess::Bitboard visitedBlack = 0; // Bitboard to track black pawns we've already processed

    int whitePawnIslands = 0;
    int blackPawnIslands = 0;

    // Calculate pawn islands for white pawns
    while (whitePawns)
    {
        chess::Bitboard currentPawn = whitePawns.pop();

        // If this pawn hasn't been visited, it's a new island
        if (!(visitedWhite & currentPawn))
        {
            whitePawnIslands++;

            // Perform flood fill to mark the entire connected component of pawns
            chess::Bitboard stack = currentPawn;
            while (stack)
            {
                chess::Bitboard current = stack.pop();
                visitedWhite |= current; // Mark as visited

                // Check for connected pawns in adjacent squares (left, right, up, down)
                if (current & NFILE_A)
                    stack |= current << 1; // Left
                if (current & NFILE_H)
                    stack |= current >> 1; // Right
                if (current & NRANK_8)
                    stack |= current << 8; // Up (white) or Down (black)
                if (current & NRANK_1)
                    stack |= current >> 8; // Down (white) or Up (black)
            }
        }
    }

    // Calculate pawn islands for black pawns
    while (blackPawns)
    {
        chess::Bitboard currentPawn = blackPawns.pop();

        // If this pawn hasn't been visited, it's a new island
        if (!(visitedBlack & currentPawn))
        {
            blackPawnIslands++;

            // Perform flood fill to mark the entire connected component of pawns
            chess::Bitboard stack = currentPawn;
            while (stack)
            {
                int current = stack.pop();
                visitedBlack |= current; // Mark as visited

                // Check for connected pawns in adjacent squares (left, right, up, down)
                if (current & NFILE_A)
                    stack |= current << 1; // Left
                if (current & NFILE_H)
                    stack |= current >> 1; // Right
                if (current & NRANK_8)
                    stack |= current << 8; // Up (white) or Down (black)
                if (current & NRANK_1)
                    stack |= current >> 8; // Down (white) or Up (black)
            }
        }
    }

    // Penalty per pawn island
    whitePenalty = whitePawnIslands * Island;
    blackPenalty = blackPawnIslands * Island;

    // Return the difference (white penalty - black penalty)
    return whitePenalty - blackPenalty;
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
    int whitePenalty = whiteHoles.count();
    int blackPenalty = blackHoles.count();

    return (whitePenalty - blackPenalty)*hole;
}
Middlegame int pawn(const chess::Position &pos)
{
    return doubled(pos) - isolated(pos) - backward(pos) - islands(pos) - holes(pos);
}
// 2.4. Mobility

AllPhases int16_t mobility(const chess::Position &board)
{
    using namespace chess;
    int mobilityScore = 0;

    Bitboard occupied = board.occ();

    const Color sides[2] = {Color::WHITE, Color::BLACK};

    for (Color side : sides)
    {
        Bitboard us = board.us(side);

        for (PieceType pt : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN})
        {
            Bitboard pieces = board.pieces(pt) & us;
            while (pieces)
            {
                Square sq = pieces.pop();

                Bitboard attacks = 0;
                switch (pt)
                {
                case (int)PieceType::KNIGHT:
                    attacks = chess::attacks::knight(sq);
                    break;
                case (int)PieceType::BISHOP:
                    attacks = chess::attacks::bishop(sq, occupied);
                    break;
                case (int)PieceType::ROOK:
                    attacks = chess::attacks::rook(sq, occupied);
                    break;
                case (int)PieceType::QUEEN:
                    attacks = chess::attacks::queen(sq, occupied);
                    break;
                default:
                    break;
                }

                // Remove friendly pieces from attack mask
                attacks &= ~us;

                int attackCount = attacks.count();
                int weighted = MOBILITY_WEIGHTS[static_cast<int>(pt)] * attackCount;

                mobilityScore += (side == Color::WHITE) ? weighted : -weighted;
            }
        }
    }

    return mobilityScore;
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
    int w = !wshieldCount1 * wshieldCount2 + wshieldCount1,
        b = !bshieldCount1 * bshieldCount2 + bshieldCount1;
    return (w - b) * pawnShield;
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

    int whiteStorm = (blackPawns & stormZone & whiteFlankMask).count();
    int blackStorm = (whitePawns & stormZone & blackFlankMask).count();

    // Positive score if Black storms White, negative if White storms Black (from White POV)
    return pawnStorm * (whiteStorm - blackStorm);
}
// 2.5.3. King near open/semi-open files
Middlegame int16_t king_near_file(const chess::Position &board)
{
    chess::Square whiteKing = board.kingSq(chess::Color::WHITE);
    chess::Square blackKing = board.kingSq(chess::Color::BLACK);

    constexpr chess::Bitboard KINGSIDE_FLANK = chess::attacks::MASK_FILE[4] | chess::attacks::MASK_FILE[5] | chess::attacks::MASK_FILE[6] | chess::attacks::MASK_FILE[7];
    constexpr chess::Bitboard QUEENSIDE_FLANK = chess::attacks::MASK_FILE[0] | chess::attacks::MASK_FILE[1] | chess::attacks::MASK_FILE[2] | chess::attacks::MASK_FILE[3];

    chess::Bitboard whitePawns = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE),
                    blackPawns = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK),
                    whiteFlankMask = (whiteKing.file() >= chess::File::FILE_E) ? KINGSIDE_FLANK : QUEENSIDE_FLANK,
                    blackFlankMask = (blackKing.file() >= chess::File::FILE_E) ? KINGSIDE_FLANK : QUEENSIDE_FLANK,
                    wopens, wsemiopens, wpotential,
                    bopens, bsemiopens, bpotential;
    // Phase 1: count open/semi-open files for both
    for (int file = 0; file < 8; ++file)
    {
        chess::Bitboard fileMask = chess::attacks::MASK_FILE[file];
        if ((whitePawns & fileMask) && (blackPawns & fileMask))
            bsemiopens |= fileMask;
        if (!(whitePawns & fileMask) && (blackPawns & fileMask))
            wsemiopens |= fileMask;
        if (!(whitePawns & fileMask) && !(blackPawns & fileMask))
        {
            wopens |= fileMask;
            bopens |= fileMask;
        }
    }
    wpotential = wopens | wsemiopens, bpotential = bopens | bsemiopens;
    return bool(whiteFlankMask & wpotential) * kingNearFile - bool(blackFlankMask & bpotential) * kingNearFile;
}
Middlegame int16_t king_safety(const chess::Position &board)
{
    return pawn_shield(board) + pawn_storm(board) + king_near_file(board);
}
// 2.6. Space
AllPhases int space(const chess::Position &board)
{
    int score = 0;

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
                    score += Space * color_multiplier;
                }
            }
        }
    }

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
// 3. Main evaluation
int16_t eg(const chess::Position& pos){
    if (draw(pos)) return 0;
    int score = space(pos) + tempo(pos) + material(pos) + passed(pos);
    return score;
}
int16_t mg(const chess::Position& pos){
    return material(pos) + space(pos) + tempo(pos) + mobility(pos) + king_safety(pos);
}
int16_t eval(const chess::Position &pos)
{
    const int phase = ::phase(pos);
    return ((mg(pos) * phase) + (eg(pos) * (256 - phase))) / 256;
}

int16_t piece_value(chess::PieceType p){
    switch((int)p){
        case (int)chess::PieceType::PAWN: return PawnValue;
        case (int)chess::PieceType::KNIGHT: return KnightValue;
        case (int)chess::PieceType::BISHOP: return BishopValue;
        case (int)chess::PieceType::ROOK: return RookValue;
        case (int)chess::PieceType::QUEEN: return QueenValue;
        default:
        return 0;
    }
}