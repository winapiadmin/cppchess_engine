#include "pawns.h"
#include "patterns.h"
#define LIGHT_SQUARES  0x55aa55aa55aa55aa
#define DARK_SQUARES   0xaa55aa55aa55aa55
int doubled(U64 whitePawns, U64 blackPawns) {
    int score = 0;

    for (int i = 0; i < 8; ++i) {
        int whiteCount = countBits(whitePawns & FILES[i]);
        int blackCount = countBits(blackPawns & FILES[i]);

        // Penalty for doubled/tripled pawns
        if (whiteCount > 1) score -= (whiteCount - 1) * 10; // -10 per extra pawn
        if (blackCount > 1) score += (blackCount - 1) * 10; // +10 per extra pawn
    }

    return score;
}

// Check for backward pawns
int backward(U64 whitePawns, U64 blackPawns) {
    int score = 0;

    for (int i = 0; i < 8; ++i) {
        U64 fileMask = FILES[i];

        U64 whiteFile = whitePawns & fileMask;
        U64 blackFile = blackPawns & fileMask;

        // Adjacent files
        U64 leftFile = (i > 0) ? FILES[i - 1] : 0;
        U64 rightFile = (i < 7) ? FILES[i + 1] : 0;

        U64 whiteAdjacent = whitePawns & (leftFile | rightFile);
        U64 blackAdjacent = blackPawns & (leftFile | rightFile);

        while (whiteFile) {
            int pos = __builtin_ctzll(whiteFile); // Get least significant bit index
            whiteFile &= whiteFile - 1; // Remove this pawn from bitboard

            // A backward pawn has no adjacent support and is behind an enemy pawn
            if (!(whiteAdjacent & (1ULL << pos)) && (blackPawns & (fileMask >> 8))) {
                score -= 15; // Penalty for white backward pawn
            }
        }

        while (blackFile) {
            int pos = __builtin_ctzll(blackFile);
            blackFile &= blackFile - 1;

            if (!(blackAdjacent & (1ULL << pos)) && (whitePawns & (fileMask << 8))) {
                score += 15; // Penalty for black backward pawn
            }
        }
    }

    return score;
}


// **1. Blockage Detection** (Pawns that cannot advance)
inline U64 getBlockedPawns(U64 pawns, U64 enemyPieces, bool isWhite) {
    return isWhite ? north(pawns) & enemyPieces : south(pawns) & enemyPieces;
}
int blockage(const chess::Board& board){
    return POPCOUNT64(getBlockedPawns(board.pieces(chess::PieceType::underlying::PAWN,board.sideToMove()).getBits(),board.them(board.sideToMove()).getBits(),(int)board.sideToMove()))*5;
}

// **3. Hanging Pawns** (Isolated and unprotected)
inline U64 getHangingPawns(U64 pawns, U64 friendlyPieces) {
    U64 defended = (east(pawns) | west(pawns) | northEast(pawns) | northWest(pawns) |
                    southEast(pawns) | southWest(pawns)) & friendlyPieces;
    return pawns & ~defended;
}

int pawnIslands(const chess::Board& board){
    
    int islands = 0;
    U64 remaining = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();

    for (int i = 0; i < 8; ++i) {
        if (remaining & FILES[i]) {
            islands++;
            remaining &= ~FILES[i]; // Remove the entire file from processing
        }
    }
	return islands*5;
}
// **7. Pawn Majority** (More pawns on one side)
inline bool hasPawnMajority(U64 pawns, bool kingside) {
    U64 sideMask = kingside ? (FILE_E | FILE_F | FILE_G | FILE_H)
                            : (FILE_A | FILE_B | FILE_C | FILE_D);
    return countBits(pawns & sideMask) > 4; // Majority is more than 4 pawns
}
#define getIsolatedPawns(pawns) pawns & ~(west(pawns) | east(pawns))
inline U64 getDoublyIsolatedPawns(U64 pawns) {
    U64 isolated = getIsolatedPawns(pawns);
    return isolated & (isolated - 1); // More than one isolated pawn in a file
}
int isolated(const chess::Board& board){
    return POPCOUNT64(getIsolatedPawns(board.pieces(chess::PieceType::underlying::PAWN,board.sideToMove()).getBits()))*5;
}
int dblisolated(const chess::Board& board){
    return POPCOUNT64(getDoublyIsolatedPawns(board.pieces(chess::PieceType::underlying::PAWN,board.sideToMove()).getBits()))*10;
}
// **2. Holes** (Uncontested squares that cannot be controlled by pawns)
inline U64 getHoles(U64 whitePawns, U64 blackPawns) {
    U64 pawnControl = north(whitePawns) | south(blackPawns);
    return ~pawnControl; // Holes are squares not controlled by any pawn
}
int holes(const chess::Board& board){
	return POPCOUNT64(getHoles(board.pieces(chess::PieceType::underlying::PAWN,chess::Color::underlying::WHITE).getBits(),board.pieces(chess::PieceType::underlying::PAWN,chess::Color::underlying::BLACK).getBits()))*5;
}

int pawnRace(const chess::Board& board) {
    U64 myPawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    int minDistance = 8; // Maximum distance to promotion
    while (myPawns) {
        int pos = __builtin_ctzll(myPawns);
        myPawns &= myPawns - 1;
        int rank = pos / 8;
        minDistance = std::min(minDistance, board.sideToMove() ? (7 - rank) : rank);
    }
    if (phase(board)==3)
    	return (8 - minDistance) * 10; // Bonus for closer pawns
    else return 8 - minDistance; // no bonus for pawns promoted in opening-middlegame due to bad opponent playing (which perfect play must went to endgame)
}
// **4. Weak Pawns (Backwards, Attackable, or Isolated)**
inline U64 getWeakPawns(U64 pawns, U64 enemyPawns) {
    U64 isolated = getIsolatedPawns(pawns);
    U64 backward = (north(pawns) & enemyPawns) | (south(pawns) & enemyPawns);
    return isolated | backward; // Any pawn that is weak
}
int weaks(const chess::Board& board){
    return POPCOUNT64(getWeakPawns(board.pieces(chess::PieceType::underlying::PAWN,board.sideToMove()).getBits(),board.pieces(chess::PieceType::underlying::PAWN,~board.sideToMove()).getBits()))*6;
}
inline U64 getKingMobility(U64 king, U64 enemyPieces) {
    U64 mobility = (north(king) | south(king) | east(king) | west(king) |
                    northEast(king) | northWest(king) | southEast(king) | southWest(king));

    return mobility & ~enemyPieces; // Remove squares occupied by enemy pieces
}
inline bool shouldUnderpromote(U64 pawn, U64 enemyKing, bool isWhite, U64 enemyPieces, U64 friendlyPieces, chess::Square promotionSquare, int gamePhase) {
    U64 kingZone = north(enemyKing) | south(enemyKing) | east(enemyKing) | west(enemyKing);

    // **1. Stalemate Prevention**
    U64 kingMobility = getKingMobility(enemyKing, enemyPieces | friendlyPieces);
    if (kingMobility == 0) return true; // If enemy king has no legal moves → underpromotion is preferred

    // **2. Tactical Fork or Mating Net Check (Knight Promotions)**
    if (chess::Square::back_rank(promotionSquare,chess::Color(isWhite))) {
        U64 knightAttacks = getKnightAttacks(promotionSquare);
        if (knightAttacks & enemyKing) return true; // If knight promotes with a check/fork, prefer underpromotion
    }

    // **3. Underpromotion in the Opening (Forcing Move Continuation)**
    if (gamePhase == 1) { // Opening phase
        if (chess::Bitboard(1ULL<<promotionSquare.index()) & enemyPieces) return true; // If underpromotion maintains initiative
    }

    // **4. Endgame Underpromotion to Avoid Immediate Loss**
    if (gamePhase == 3) { // Endgame phase
        if (isLosingQueenPromotion(pawn, enemyKing, enemyPieces, friendlyPieces)) return true; // Syzygy-based check
    }

    // **5. Avoiding Forks (Critical Defensive Underpromotion)**
    if (enemyPieces & getKnightAttacks(promotionSquare)) {
        return true; // Underpromotion is needed to avoid losing material
    }

    return false; // Default: Queen promotion is optimal
}
int underpromote(chess::Board &board) {
    if (board.move_stack.empty()) return 0;  // No moves to check

    chess::Move lastMove = board.move_stack.back();  // Get last move

    // Ensure move is valid
    if (lastMove.from() == chess::Square::underlying::NO_SQ || lastMove.to() == chess::Square::underlying::NO_SQ) {
        return 0;  // Invalid move → no underpromotion evaluation
    }

    // 🔥 Temporarily undo move to access the original piece
    board.pop();  // Undo the last move

    // Get the original piece before promotion
    chess::Piece movedPiece = board.at(lastMove.from());

    // Ensure it was a pawn move
    bool wasPawn = (movedPiece == chess::Piece::WHITEPAWN || movedPiece == chess::Piece::BLACKPAWN);

    // Precompute board properties to **avoid recomputation**
    U64 pawnBB = 1ULL << lastMove.from().index();
    U64 enemyKingBB = 1ULL << board.kingSq(~board.sideToMove()).index();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();
    U64 friendlyPieces = board.us(board.sideToMove()).getBits();
    bool isWhite = (board.sideToMove() == chess::Color::WHITE);
    int gamePhase = phase(board);

    // Compute if underpromotion was correct
    bool shouldUnderpromoteResult = shouldUnderpromote(
        pawnBB, enemyKingBB, isWhite, enemyPieces, friendlyPieces, lastMove.to(), gamePhase
    );

    bool underpromoted = lastMove.promotionType().internal() != chess::PieceType::underlying::QUEEN;

    // Redo the move immediately to restore board state
    board.makeMove(lastMove);

    // If it wasn't a pawn, ignore the check
    if (!wasPawn) return 0;

    // Check if move was a promotion
    if (lastMove.promotionType() == chess::PieceType::underlying::NONE) {
        return 0;  // No promotion → no underpromotion penalty
    }

    return (shouldUnderpromoteResult && underpromoted) ? 0 : 50;  // Less harsh penalty
}

// **2. Color Weakness (Missing Pawn Protection)**
inline U64 getColorWeakness(U64 pawns) {
    return ~((pawns & DARK_SQUARES) | (pawns & LIGHT_SQUARES)); // Weak squares
}
int weakness(const chess::Board& board){
    return POPCOUNT64(getColorWeakness(board.pieces(chess::PieceType::underlying::PAWN,board.sideToMove()).getBits()))*10;
}
// **5. Pawn Shield (King Protection by Pawns)**
inline U64 getPawnShield(U64 king, U64 pawns, bool isWhite) {
    return isWhite ? (north(king) & pawns) : (south(king) & pawns);
}
int pawnShield(const chess::Board& board){
    return POPCOUNT64(getPawnShield(1ULL<<board.kingSq(board.sideToMove()).index(),board.pieces(chess::PieceType::underlying::PAWN,board.sideToMove()).getBits(),(int)board.sideToMove()))*5;
}

// **7. Outposts (Strong Knight Squares)**
inline U64 getOutposts(U64 knights, U64 pawns, bool isWhite) {
    U64 support = isWhite ? south(knights) & pawns : north(knights) & pawns;
    return support & ~(north(pawns) | south(pawns)); // Supported and no pawn challenge
}

// **9. Pawn Levers (Pawns that can break the structure)**
inline U64 getPawnLevers(U64 pawns, U64 enemyPawns) {
    return (east(pawns) & enemyPawns) | (west(pawns) & enemyPawns);
}

// **10. Pawn Rams (Locked Pawns)**
// Pawns that are directly blocking each other.
inline U64 getPawnRams(U64 pawns, U64 enemyPawns) {
    return (north(pawns) & south(enemyPawns)) | (south(pawns) & north(enemyPawns));
}

// **11. Unfree Pawns (Blocked Pawns)**
// Pawns that cannot advance and are not attackable forward.
inline U64 getUnfreePawns(U64 pawns, U64 enemyPawns) {
    U64 blocked = north(pawns) & enemyPawns; // Blocked forward
    U64 diagonals = (northEast(pawns) | northWest(pawns)) & enemyPawns; // Blocked diagonally
    return blocked & diagonals; // Pawn is fully restricted
}

// **12. Open Pawns (Pawns that have an open file ahead)**
inline U64 getOpenPawns(U64 pawns, U64 enemyPawns) {
    return pawns & ~(north(pawns) & enemyPawns); // No enemy directly in front
}

int pawnStorm(const chess::Board& board) {
    U64 myPawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 storm = board.sideToMove() ? north(myPawns) | north(north(myPawns))
                                  : south(myPawns) | south(south(myPawns));
    return POPCOUNT64(storm) * 5; // Bonus for aggressive pawn pushes
}

int pawnLevers(const chess::Board& board) {
    U64 myPawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    return POPCOUNT64(getPawnLevers(myPawns, enemyPawns)) * 5; // Bonus for break opportunities
}

int outpost(const chess::Board& board) {
    U64 myKnights = board.pieces(chess::PieceType::KNIGHT, board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    return POPCOUNT64(getOutposts(myKnights, myPawns, board.sideToMove())) * 10; // Reward strong knights
}
int evaluatePawnRams(const chess::Board& board) {
    return POPCOUNT64(getPawnRams(
        board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits(),
        board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits())) * 5;
}

int evaluateUnfreePawns(const chess::Board& board) {
    return POPCOUNT64(getUnfreePawns(
        board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits(),
        board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits())) * 7;
}

int evaluateOpenPawns(const chess::Board& board) {
    return POPCOUNT64(getOpenPawns(
        board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits(),
        board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits())) * 3;
}
