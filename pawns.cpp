#include "pawns.h"
#include "patterns.h"

#define LIGHT_SQUARES  0x55aa55aa55aa55aaULL
#define DARK_SQUARES   0xaa55aa55aa55aa55ULL

// **1. Doubled Pawns Penalty**
int doubled(U64 whitePawns, U64 blackPawns) {
    int score = 0;
    for (int i = 0; i < 8; ++i) {
        int whiteCount = __builtin_popcountll(whitePawns & FILES[i]);
        int blackCount = __builtin_popcountll(blackPawns & FILES[i]);

        if (whiteCount > 1) score -= (whiteCount - 1) * 10;
        if (blackCount > 1) score += (blackCount - 1) * 10;
    }
    return score;
}

// **2. Backward Pawns**
int backward(U64 whitePawns, U64 blackPawns) {
    int score = 0;
    for (int i = 0; i < 8; ++i) {
        U64 fileMask = FILES[i], leftFile = (i > 0) ? FILES[i - 1] : 0, rightFile = (i < 7) ? FILES[i + 1] : 0;
        U64 whiteFile = whitePawns & fileMask, blackFile = blackPawns & fileMask;
        U64 whiteAdjacent = whitePawns & (leftFile | rightFile), blackAdjacent = blackPawns & (leftFile | rightFile);

        while (whiteFile) {
            int pos = __builtin_ctzll(whiteFile);
            whiteFile &= whiteFile - 1;
            if (!(whiteAdjacent & (1ULL << pos)) && (blackPawns & (fileMask >> 8))) score -= 15;
        }
        while (blackFile) {
            int pos = __builtin_ctzll(blackFile);
            blackFile &= blackFile - 1;
            if (!(blackAdjacent & (1ULL << pos)) && (whitePawns & (fileMask << 8))) score += 15;
        }
    }
    return score;
}

// **3. Pawn Blockage**
int blockage(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();
    return __builtin_popcountll((board.sideToMove() ? north(pawns) : south(pawns)) & enemyPieces) * 5;
}

// **4. Pawn Islands**
int pawnIslands(const chess::Board& board) {
    int islands = 0;
    U64 remaining = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    for (int i = 0; i < 8; ++i) {
        if (remaining & FILES[i]) {
            islands++;
            remaining &= ~FILES[i];
        }
    }
    return islands * 5;
}

// **5. Isolated & Doubly Isolated Pawns**
int isolated(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    return __builtin_popcountll(pawns & ~(west(pawns) | east(pawns))) * 5;
}
int dblisolated(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 isolated = pawns & ~(west(pawns) | east(pawns));
    return __builtin_popcountll(isolated & (isolated - 1)) * 10;
}

// **6. Weak Pawns**
int weaks(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    U64 weakPawns = (pawns & ~(west(pawns) | east(pawns))) | ((north(pawns) & enemyPawns) | (south(pawns) & enemyPawns));
    return __builtin_popcountll(weakPawns) * 6;
}

// **7. Pawn Race**
int pawnRace(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    int minDistance = 8;
    while (pawns) {
        int pos = __builtin_ctzll(pawns);
        pawns &= pawns - 1;
        int rank = pos / 8;
        minDistance = std::min(minDistance, board.sideToMove() ? (7 - rank) : rank);
    }
    return (phase(board) == 3) ? (8 - minDistance) * 10 : 8 - minDistance;
}

// **8. Color Weakness**
int weakness(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    return __builtin_popcountll(~((pawns & DARK_SQUARES) | (pawns & LIGHT_SQUARES))) * 10;
}

// **9. Pawn Shield**
int pawnShield(const chess::Board& board) {
    U64 kingBB = 1ULL << board.kingSq(board.sideToMove()).index();
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    return __builtin_popcountll(board.sideToMove() ? north(kingBB) & pawns : south(kingBB) & pawns) * 5;
}

// **10. Pawn Storm**
int pawnStorm(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 storm = board.sideToMove() ? (north(pawns) | north(north(pawns))) : (south(pawns) | south(south(pawns)));
    return __builtin_popcountll(storm) * 5;
}

// **11. Outposts**
int outpost(const chess::Board& board) {
    U64 knights = board.pieces(chess::PieceType::KNIGHT, board.sideToMove()).getBits();
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 support = board.sideToMove() ? (south(knights) & pawns) : (north(knights) & pawns);
    return __builtin_popcountll(support & ~(north(pawns) | south(pawns))) * 10;
}

// **12. Pawn Levers**
int pawnLevers(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    return __builtin_popcountll((east(pawns) & enemyPawns) | (west(pawns) & enemyPawns)) * 5;
}

// **13. Pawn Rams**
int evaluatePawnRams(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    return __builtin_popcountll((north(pawns) & south(enemyPawns)) | (south(pawns) & north(enemyPawns))) * 5;
}
inline bool shouldUnderpromote(U64 pawn, U64 enemyKing, bool isWhite, U64 enemyPieces, U64 friendlyPieces, chess::Square promotionSquare, int gamePhase) {
    // **1. Stalemate Prevention** (Avoid promoting if it causes stalemate)
    U64 kingMobility = (north(enemyKing) | south(enemyKing) | east(enemyKing) | west(enemyKing) |
                        northEast(enemyKing) | northWest(enemyKing) | southEast(enemyKing) | southWest(enemyKing)) 
                        & ~(enemyPieces | friendlyPieces);
    if (kingMobility == 0) return true; // No legal moves → prefer underpromotion

    // **2. Tactical Fork or Mating Net Check (Prefer Knight underpromotion)**
    if (chess::Square::back_rank(promotionSquare, chess::Color(isWhite))) {
        U64 knightAttacks = getKnightAttacks(promotionSquare);
        if (knightAttacks & enemyKing) return true; // If a knight can deliver a fork/check, prefer underpromotion
    }

    // **3. Underpromotion in the Opening (Forcing Move Continuation)**
    if (gamePhase == 1) { // Opening phase
        if (chess::Bitboard(1ULL<<promotionSquare.index()) & enemyPieces) return true; // If underpromotion keeps initiative
    }

    // **4. Endgame Considerations (Avoid Losing Promotion)**
    if (gamePhase == 3) { // Endgame phase
        if (isLosingQueenPromotion(pawn, enemyKing, enemyPieces, friendlyPieces)) return true;
    }

    // **5. Avoiding Forks (Critical Defensive Underpromotion)**
    if (enemyPieces & getKnightAttacks(promotionSquare)) {
        return true; // Underpromotion is needed to avoid material loss
    }

    return false; // Default: Queen promotion is optimal
}
// **1. Unfree Pawns (Blocked Pawns)**
int evaluateUnfreePawns(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    U64 blocked = (north(pawns) & enemyPawns) | (south(pawns) & enemyPawns);
    return __builtin_popcountll(blocked) * 7; // Higher penalty for blocked pawns
}

// **2. Open Pawns (Pawns with no obstacles in front)**
int evaluateOpenPawns(const chess::Board& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    U64 openFiles = pawns & ~(north(pawns) & enemyPawns);
    return __builtin_popcountll(openFiles) * 3; // Slight reward for open files
}

// **3. Holes (Uncontested squares that cannot be controlled by pawns)**
int holes(const chess::Board& board) {
    U64 whitePawns = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE).getBits();
    U64 blackPawns = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK).getBits();
    U64 pawnControl = north(whitePawns) | south(blackPawns);
    return __builtin_popcountll(~pawnControl) * 5; // Penalize holes in structure
}

// **4. Underpromotion Decision**
int underpromote(chess::Board& board) {
    if (board.move_stack.empty()) return 0; // No move history → no underpromotion check

    chess::Move lastMove = board.move_stack.back(); // Get last move
    if (lastMove.from() == chess::Square::underlying::NO_SQ || lastMove.to() == chess::Square::underlying::NO_SQ) {
        return 0; // Invalid move → no underpromotion evaluation
    }

    // Undo move temporarily to fetch piece before promotion
    board.pop();
    chess::Piece movedPiece = board.at(lastMove.from());

    // Ensure it was a pawn move
    bool wasPawn = (movedPiece == chess::Piece::WHITEPAWN || movedPiece == chess::Piece::BLACKPAWN);

    // Precompute necessary board properties
    U64 pawnBB = 1ULL << lastMove.from().index();
    U64 enemyKingBB = 1ULL << board.kingSq(~board.sideToMove()).index();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();
    U64 friendlyPieces = board.us(board.sideToMove()).getBits();
    bool isWhite = (board.sideToMove() == chess::Color::WHITE);
    int gamePhase = phase(board);

    // Check if underpromotion was tactically beneficial
    bool shouldUnderpromoteResult = shouldUnderpromote(
        pawnBB, enemyKingBB, isWhite, enemyPieces, friendlyPieces, lastMove.to(), gamePhase
    );

    bool underpromoted = lastMove.promotionType().internal() != chess::PieceType::QUEEN;

    // Restore move
    board.makeMove(lastMove);

    if (!wasPawn) return 0; // Ignore non-pawn moves
    if (lastMove.promotionType() == chess::PieceType::NONE) return 0; // Not a promotion

    return (shouldUnderpromoteResult && underpromoted) ? 0 : 50; // Adjust penalty
}
