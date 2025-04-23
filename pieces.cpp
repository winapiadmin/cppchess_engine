#include "pieces.h"
// Piece weights for determining game phase
constexpr int PAWN_PHASE    = 0;
constexpr int KNIGHT_PHASE  = 1;
constexpr int BISHOP_PHASE  = 1;
constexpr int ROOK_PHASE    = 2;
constexpr int QUEEN_PHASE   = 4;

// Initial game phase score (sum of all piece values)
constexpr int TOTAL_PHASE = (KNIGHT_PHASE * 4) + (BISHOP_PHASE * 4) +
                            (ROOK_PHASE * 4) + (QUEEN_PHASE * 2);

// Calculate game phase based on remaining pieces
int getGamePhase(int whiteKnights, int whiteBishops, int whiteRooks, int whiteQueens,
                 int blackKnights, int blackBishops, int blackRooks, int blackQueens) {
    
    int currentPhase = TOTAL_PHASE - (
        (whiteKnights + blackKnights) * KNIGHT_PHASE +
        (whiteBishops + blackBishops) * BISHOP_PHASE +
        (whiteRooks + blackRooks) * ROOK_PHASE +
        (whiteQueens + blackQueens) * QUEEN_PHASE
    );

    double phaseRatio = (double)currentPhase / TOTAL_PHASE;

    if (phaseRatio >= 0.7) {
        return 1; // Opening
    } else if (phaseRatio >= 0.3) {
        return 2; // Middlegame
    } else {
        return 3; // Endgame
    }
}
int phase(const chess::Board& board) {
    int whiteKnights = POPCOUNT64(board.pieces(chess::PieceType::underlying::KNIGHT, chess::Color::underlying::WHITE).getBits());
    int whiteBishops = POPCOUNT64(board.pieces(chess::PieceType::underlying::BISHOP, chess::Color::underlying::WHITE).getBits());
    int whiteRooks = POPCOUNT64(board.pieces(chess::PieceType::underlying::ROOK, chess::Color::underlying::WHITE).getBits());
    int whiteQueens = POPCOUNT64(board.pieces(chess::PieceType::underlying::QUEEN, chess::Color::underlying::WHITE).getBits());

    int blackKnights = POPCOUNT64(board.pieces(chess::PieceType::underlying::KNIGHT, chess::Color::underlying::BLACK).getBits());
    int blackBishops = POPCOUNT64(board.pieces(chess::PieceType::underlying::BISHOP, chess::Color::underlying::BLACK).getBits());
    int blackRooks = POPCOUNT64(board.pieces(chess::PieceType::underlying::ROOK, chess::Color::underlying::BLACK).getBits());
    int blackQueens = POPCOUNT64(board.pieces(chess::PieceType::underlying::QUEEN, chess::Color::underlying::BLACK).getBits());

    return getGamePhase(whiteKnights, whiteBishops, whiteRooks, whiteQueens,
                        blackKnights, blackBishops, blackRooks, blackQueens);
}

constexpr U64 DARK_SQUARES = 0xAA55AA55AA55AA55ULL; // Dark square mask
constexpr U64 LIGHT_SQUARES = ~DARK_SQUARES; // Light square mask
// **1. Bad Bishop Detection**
inline bool isBadBishop(U64 bishop, U64 pawns, bool isWhite) {
    U64 sameColorPawns = (bishop & DARK_SQUARES) ? (pawns & DARK_SQUARES) : (pawns & LIGHT_SQUARES);
    return countBits(sameColorPawns) > 3; // More than 3 blocking pawns = bad bishop
}


// **3. King Pawn Tropism (King Close to Pawns)**
inline int kingPawnTropism(U64 king, U64 enemyPawns) {
    return countBits((north(enemyPawns) | south(enemyPawns) | east(enemyPawns) | west(enemyPawns)) & king);
}

// **4. King Safety (Pawn Cover)**
inline int getKingSafety(U64 king, U64 pawns) {
    return countBits((north(king) | east(king) | west(king)) & pawns);
}

// **8. Rook on Open/Semi-Open Files**
inline bool isRookOnOpenFile(U64 rook, U64 pawns) {
    for (int i = 0; i < 8; ++i) {
        if (rook & FILES[i]) {
            return !(pawns & FILES[i]); // No pawns on file = open
        }
    }
    return false;
}

inline bool isRookOnSemiOpenFile(U64 rook, U64 pawns, bool isWhite) {
    for (int i = 0; i < 8; ++i) {
        if (rook & FILES[i]) {
            return countBits(pawns & FILES[i]) == 1; // One pawn = semi-open
        }
    }
    return false;
}

// **1. Fianchetto Detection**
inline bool isFianchetto(U64 bishop, U64 pawns, bool isWhite) {
    return (isWhite && (bishop & (1ULL << 6))) && ((pawns & (1ULL << 5)) && (pawns & (1ULL << 7)))
        || (isWhite && (bishop & (1ULL << 2))) && ((pawns & (1ULL << 1)) && (pawns & (1ULL << 3)))
        || (!isWhite && (bishop & (1ULL << 62))) && ((pawns & (1ULL << 61)) && (pawns & (1ULL << 63)))
        || (!isWhite && (bishop & (1ULL << 58))) && ((pawns & (1ULL << 57)) && (pawns & (1ULL << 59)));
}

// **3. Returning Bishop (Retreat to Safety)**
inline U64 getReturningBishops(U64 bishops, U64 attacks) {
    return bishops & ~attacks; // Bishops with escape squares
}

// **4. Trapped Pieces (Limited Mobility)**
inline U64 getTrappedPieces(U64 pieces, U64 attacks) {
    return pieces & attacks; // Pieces with no escape
}

// **5. King Safety Pattern**
inline int evaluateKingSafety(U64 king, U64 enemyPieces) {
    U64 dangerZone = king << 8 | king >> 8 | king << 1 | king >> 1;
    return countBits(dangerZone & enemyPieces);
}

// **6. Mate at a Glance (Basic Mate Patterns)**
inline bool detectQuickMate(U64 king, U64 queen, U64 rooks, U64 knights, U64 enemyKing) {
    return (queen & enemyKing) || ((rooks & enemyKing) && (knights & enemyKing));
}

// **8. King Patterns (Mobility & Escape Routes)**
inline U64 getKingMobility(U64 king, U64 friendlyPieces) {
    return (king << 8 | king >> 8 | king << 1 | king >> 1) & ~friendlyPieces;
}

// King Safety Zones
constexpr U64 KING_SIDE = FILE_G | FILE_H;
constexpr U64 QUEEN_SIDE = FILE_A | FILE_B;
constexpr U64 CENTER = (FILE_D | FILE_E) & (RANK_4 | RANK_5);

inline int firstSetBit(U64 bb) { return bb ? __builtin_ctzll(bb) : -1; }

// **1. Detect Trapped Pieces**
U64 getTrappedPieces(U64 pieces, U64 pawns, U64 enemyAttacks) {
    U64 trapped = 0;

    while (pieces) {
        int pos = firstSetBit(pieces);
        pieces &= pieces - 1; // Remove processed piece

        U64 mask = 1ULL << pos;
        U64 escapeMoves = (mask << 8) | (mask >> 8) | (mask << 1) | (mask >> 1);
        
        if ((escapeMoves & pawns) || (escapeMoves & enemyAttacks)) {
            trapped |= mask;
        }
    }

    return trapped;
}

// **2. King Safety Evaluation**
int evaluateKingSafety(U64 king, U64 ownPawns, U64 enemyAttacks) {
    int score = 0;
    
    if (king & KING_SIDE) score -= 10; // King on a flank is more exposed
    if (king & QUEEN_SIDE) score -= 5; // King slightly safer
    
    U64 pawnShield = (king << 8) & ownPawns;
    U64 enemyPressure = (king >> 8) & enemyAttacks;

    score -= 15 * countBits(~pawnShield & enemyPressure); // Penalize weak king cover

    return score;
}

// **3. Space Control Evaluation**
int evaluateSpace(U64 ownPawns, U64 enemyPawns, bool isWhite) {
    U64 forwardArea = isWhite ? ownPawns & (RANK_4 | RANK_5) : ownPawns & (RANK_5 | RANK_7);
    return 5 * countBits(forwardArea); // More space = better position
}

// **4. Real Tempo Detection**
int evaluateTempo(U64 ownMoves, U64 forcedMoves, U64 enemyForcedMoves) {
    int freeMoves = countBits(ownMoves & ~forcedMoves);
    int opponentForced = countBits(enemyForcedMoves);
    return (freeMoves - opponentForced) * 10;
}

int msb(Bitboard bb)
{
#ifdef __GNUC__ // GCC/Clang
    return bb ? 63 - __builtin_clzll(bb) : -1;
#else
    // Fallback for compilers without __builtin_clzll
    if (bb == 0) return -1;
    int index = 0;
    while (bb >>= 1)
    {
        index++;
    }
    return index;
#endif
}

std::vector<Square> scan_reversed(Bitboard bb)
{
    std::vector<Square> iter;
    iter.reserve(64);
#ifdef __GNUC__ // GCC/Clang optimization
    while (bb)
    {
        int square = 63 - __builtin_clzll(bb); // Use intrinsic to find MSB
        iter.push_back(square);
        bb &= ~(1ULL << square); // Clear the MSB
    }
#else
    while (bb)
    {
        int square = msb(bb); // Use the optimized msb function
        iter.push_back(square);
        bb &= ~(1ULL << square); // Clear the MSB
    }
#endif
    return iter;
}
std::unordered_map<U64, int> bishopCache;

// **1. Evaluate Bad Bishops**
int evaluateBadBishops(const chess::Board& board) {
    U64 hash = board.hash();
    if (bishopCache.find(hash) != bishopCache.end()) return bishopCache[hash];
    U64 myBishops = board.pieces(chess::PieceType::underlying::BISHOP, board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    
    int score = 0;
    while (myBishops) {
        int pos = __builtin_ctzll(myBishops);
        myBishops &= myBishops - 1;
        if (isBadBishop(1ULL << pos, myPawns, board.sideToMove())) {
            score -= 10; // Penalty for bad bishop
        }
    }
    bishopCache[hash] = score;
    return score;
}
// **2. Evaluate King Safety**
int evaluateKingSafety(const chess::Board& board) {
    U64 king = 1ULL << board.kingSq(board.sideToMove()).index();
    U64 ownPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();

    return getKingSafety(king, ownPawns) * 5 - evaluateKingSafety(king, ownPawns, enemyPieces);
}

// **3. Evaluate King Pawn Tropism**
int evaluateKingPawnTropism(const chess::Board& board) {
    U64 king = 1ULL << board.kingSq(board.sideToMove()).index();
    U64 enemyPawns = board.pieces(chess::PieceType::underlying::PAWN, ~board.sideToMove()).getBits();
    return kingPawnTropism(king, enemyPawns) * -3; // Penalize proximity to enemy pawns
}

// **4. Evaluate Rooks on Open/Semi-Open Files**
int evaluateRooksOnFiles(const chess::Board& board) {
    U64 myRooks = board.pieces(chess::PieceType::underlying::ROOK, board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    
    int score = 0;
    while (myRooks) {
        int pos = __builtin_ctzll(myRooks);
        myRooks &= myRooks - 1;
        
        U64 rook = 1ULL << pos;
        if (isRookOnOpenFile(rook, myPawns)) score += 15;
        else if (isRookOnSemiOpenFile(rook, myPawns, board.sideToMove())) score += 7;
    }
    return score;
}

// **5. Evaluate Fianchetto Bishops**
int evaluateFianchetto(const chess::Board& board) {
    U64 myBishops = board.pieces(chess::PieceType::underlying::BISHOP, board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    
    int score = 0;
    while (myBishops) {
        int pos = __builtin_ctzll(myBishops);
        myBishops &= myBishops - 1;
        if (isFianchetto(1ULL << pos, myPawns, board.sideToMove())) {
            score += 10; // Bonus for fianchetto structure
        }
    }
    return score;
}

// **6. Evaluate Trapped Pieces**
int evaluateTrappedPieces(const chess::Board& board) {
    U64 myPieces = board.us(board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    U64 enemyAttacks = board.them(~board.sideToMove()).getBits();

    return POPCOUNT64(getTrappedPieces(myPieces, myPawns, enemyAttacks)) * 10;
}

int evaluateKnightForks(const chess::Board& board) {
    U64 myKnights = board.pieces(chess::PieceType::KNIGHT, board.sideToMove()).getBits();
    chess::Bitboard enemyPieces = board.them(board.sideToMove());

    int forkScore = 0;

    // Iterate over each knight
    for (int knightSq : scan_reversed(myKnights)) {
        chess::Bitboard attacks = getKnightAttacks(chess::Square(knightSq));
        int attackedPieces = (attacks & enemyPieces).count();

        if (attackedPieces >= 2) { // Only count forks if at least two enemy pieces are attacked
            forkScore += attackedPieces * 7;
        }
    }

    return forkScore;
}


// **8. Evaluate King Mobility**
int evaluateKingMobility(const chess::Board& board) {
    U64 king = 1ULL << board.kingSq(board.sideToMove()).index();
    U64 myPieces = board.us(board.sideToMove()).getBits();

    return POPCOUNT64(getKingMobility(king, myPieces)) * 3;
}

// **9. Evaluate Space Control**
int evaluateSpaceControl(const chess::Board& board) {
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::underlying::PAWN, ~board.sideToMove()).getBits();

    return evaluateSpace(myPawns, enemyPawns, board.sideToMove());
}
int evaluateTempo(chess::Board& board) {
    chess::Movelist myMoves, opponentMoves;
    
    // Generate all legal moves for both sides
    chess::movegen::legalmoves(myMoves, board);
    
    board.makeNullMove();  // Temporarily switch turn
    chess::movegen::legalmoves(opponentMoves, board);
    board.unmakeNullMove();  // Restore the board state

    int myMoveCount = myMoves.size();
    int opponentMoveCount = opponentMoves.size();

    // Forced moves (when in check)
    int forcedMoves = board.inCheck() ? myMoveCount : 0;

    // Final tempo calculation
    return (myMoveCount - forcedMoves) * 5 - opponentMoveCount * 3;
}
