#include "patterns.h"

// **1. Back Rank Mate Detection**
inline bool isBackRankMate(U64 king, U64 enemyRooks, U64 enemyQueens, bool isWhite) {
    U64 backRank = isWhite ? RANK_1 : RANK_8;
    return (king & backRank) && ((enemyRooks | enemyQueens) & backRank);
}

// **2. Skewer Detection**
inline bool isSkewer(U64 attacker, U64 target, U64 behindTarget) {
    return (attacker & target) && (behindTarget & target);
}

// **3. Discovered Attack/Check**
inline bool isDiscoveredAttack(U64 movingPiece, U64 attacker, U64 target) {
    U64 afterMove = movingPiece & ~attacker; // Simulate moving the piece
    return (afterMove & target); // If the line opens, it's a discovered attack
}

// **4. X-Ray Attack Detection**
inline bool isXRayAttack(U64 attacker, U64 blocker, U64 target) {
    return (attacker & blocker) && (attacker & target);
}
inline bool isTrappedPiece(const chess::Board& board, chess::Square pieceSq) {
    chess::Piece piece = board.at(pieceSq);
    if (piece == chess::Piece::NONE) return false; // No piece → not trapped

    chess::Color side = piece.color();
    chess::PieceType type = piece.type();

    // **1. Generate legal moves for the piece**
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    // **2. If the piece has no legal moves and is attacked, it is trapped**
    if (moves.empty() && !chess::attacks::attackers(board, ~side, pieceSq).empty()) {
        return true;
    }

    // **3. Special Conditions for Specific Pieces**
    switch (type) {
        case chess::KNIGHT:
            // A knight is trapped if it has **no safe squares to jump to**
            for (chess::Square sq : scan_reversed(getKnightAttacks(1ULL << pieceSq.index(),board.occ()))) {
                if (board.at(sq) == chess::Piece::NONE || board.at(sq).color() != side) {
                    return false; // The knight has a safe escape
                }
            }
            return true;

        case chess::BISHOP:
            // A bishop is trapped if **all diagonal escape squares are blocked**
            for (chess::Square sq : scan_reversed(getBishopAttacks(pieceSq, board.occ()))) {
                if (board.at(sq) == chess::Piece::NONE || board.at(sq).color() != side) {
                    return false;
                }
            }
            return true;

        case chess::ROOK:
            // A rook is trapped if **all rank/file escape squares are blocked**
            for (chess::Square sq : scan_reversed(getRookAttacks(1ULL<<pieceSq.index(), board.occ()))) {
                if (board.at(sq) == chess::Piece::NONE || board.at(sq).color() != side) {
                    return false;
                }
            }
            return true;

        case chess::QUEEN:
            // A queen is trapped if **both bishop and rook movements are blocked**
            for (chess::Square sq : scan_reversed(getQueenAttacks(pieceSq, board.occ()))) {
                if (board.at(sq) == chess::Piece::NONE || board.at(sq).color() != side) {
                    return false;
                }
            }
            return true;

        case chess::KING:
            // A king is trapped if it is completely surrounded **and attacked**
            return moves.empty() && !chess::attacks::attackers(board, ~side, pieceSq).empty();

        default:
            return false; // Pawns and other cases do not need special checks
    }
}


inline bool isPinned(chess::Board& board, chess::Square pieceSq) {
    chess::Color side = board.at(pieceSq).color();
    chess::Square kingSq = board.kingSq(side);

    board.makeNullMove();
    bool isPinned = !chess::attacks::attackers(board, ~side, kingSq).empty();
    board.unmakeNullMove();

    return isPinned;
}

int evaluateTactics(const chess::Board& board) {
    int score = 0;

    chess::Square myKing = board.kingSq(board.sideToMove());
    chess::Square enemyKing = board.kingSq(~board.sideToMove());

    // **1. Pin Check**
    for (chess::Square pieceSq : scan_reversed(board.us(board.sideToMove()).getBits())) {
        if (isPinned(const_cast<chess::Board&>(board), pieceSq)) score -= 30;
    }

    // **2. Skewer Check** (Updated)
    for (chess::Square attackerSq : scan_reversed(board.pieces(chess::PieceType::ROOK, board.sideToMove()).getBits() |
                                                  board.pieces(chess::PieceType::QUEEN, board.sideToMove()).getBits())) {
        for (chess::Square targetSq : scan_reversed(board.them(board.sideToMove()).getBits())) {
            for (chess::Square behindTargetSq : scan_reversed(board.them(board.sideToMove()).getBits())) {
                if (isSkewer(1ULL<<attackerSq.index(), 1ULL<<targetSq.index(), 1ULL<<behindTargetSq.index())) {
                    score += 40;  // Skewer usually wins material
                }
            }
        }
    }

    // **3. Discovered Attack Check** (Updated)
    for (chess::Square movingSq : scan_reversed(board.us(board.sideToMove()).getBits())) {
        for (chess::Square attackerSq : scan_reversed(board.pieces(chess::PieceType::underlying::ROOK, board.sideToMove()).getBits() |
                                                      board.pieces(chess::PieceType::underlying::BISHOP, board.sideToMove()).getBits() |
                                                      board.pieces(chess::PieceType::underlying::QUEEN, board.sideToMove()).getBits())) {
            for (chess::Square targetSq : scan_reversed(board.them(board.sideToMove()).getBits())) {
                if (isDiscoveredAttack(1ULL<<movingSq.index(), 1ULL<<attackerSq.index(), 1ULL<<targetSq.index())) {
                    score += 30;  // Discovered attacks are strong
                }
            }
        }
    }

    // **4. X-Ray Attack**
    for (chess::Square pieceSq : scan_reversed(board.pieces(chess::PieceType::underlying::ROOK, board.sideToMove()).getBits() |
                                               board.pieces(chess::PieceType::underlying::QUEEN, board.sideToMove()).getBits())) {
        for (chess::Square enemyPieceSq : scan_reversed(board.them(board.sideToMove()).getBits())) {
            if (isXRayAttack(board.pieces(chess::PieceType::underlying::QUEEN, board.sideToMove()).getBits(),
                             board.pieces(chess::PieceType::underlying::ROOK, board.sideToMove()).getBits(),
                             1ULL<<enemyPieceSq.index())) {
                score += 10;
            }
        }
    }
    return score;
}


// **8. King Patterns (Mobility & Escape Routes)**
inline U64 getKingMobility(U64 king, U64 friendlyPieces) {
    return (king << 8 | king >> 8 | king << 1 | king >> 1) & ~friendlyPieces;
}

bool isLosingQueenPromotion(int pawn, U64 enemyKing, U64 enemyPieces, U64 friendlyPieces) {    
    // **1. Stalemate Check**
    U64 kingMobility = getKingMobility(enemyKing, enemyPieces | friendlyPieces);
    if (kingMobility == 0) return true; // No legal king moves → stalemate

    // **2. Immediate Fork or Skewer Check**
    U64 queen = 1ULL << pawn; // Assume queen is placed here
    if (getKnightAttacks(chess::Square(pawn)) & enemyPieces) return true; // If knight forks queen, avoid queen promotion
    if (isXRayAttack(queen, enemyPieces, enemyKing)) return true; // If queen is X-Rayed or skewered, avoid promotion

    // **3. Tablebase Draw Check (if available)**
    //if (isEndgameTablebaseDraw(friendlyPieces | queen, enemyPieces, enemyKing)) return true;

    return false; // Default: Queen promotion is safe
}
