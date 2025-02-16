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

    // A knight is trapped if it has **no safe squares to jump to**
    U64 pieceBB = 1ULL << pieceSq.index();  // Precompute once
	U64 knightAttacks = getKnightAttacks(pieceBB, board.occ()), bishopAttacks=getBishopAttacks(pieceSq, board.occ()),
	    rookAttacks = getRookAttacks(1ULL<<pieceSq.index(), board.occ()), queenAttacks=getQueenAttacks(pieceSq, board.occ());
	chess::Piece p;
    // **3. Special Conditions for Specific Pieces**
    switch (type) {
        case chess::KNIGHT:
			while (knightAttacks) {
    			int sq = __builtin_ctzll(knightAttacks);  // Extract LSB (fast)
    			p=board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false; // The knight has a safe escape
                }
    			knightAttacks &= knightAttacks - 1;  // Remove LSB
			}
            return true;

        case chess::BISHOP:
			while (bishopAttacks) {
    			int sq = __builtin_ctzll(bishopAttacks);  // Extract LSB (fast)
    			p=board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false;
                }
    			bishopAttacks &= bishopAttacks - 1;  // Remove LSB
			}
            return true;

        case chess::ROOK:
			while (rookAttacks) {
    			int sq = __builtin_ctzll(rookAttacks);  // Extract LSB (fast)
    			p=board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false;
                }
    			rookAttacks &= rookAttacks - 1;  // Remove LSB
            }
            return true;

        case chess::QUEEN:
			while (queenAttacks) {
    			int sq = __builtin_ctzll(queenAttacks);  // Extract LSB (fast)
    			p=board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false;
                }
    			queenAttacks &= queenAttacks - 1;  // Remove LSB
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
std::unordered_map<U64, int> tacticsCache;
int evaluateTactics(const chess::Board& board) {
    U64 hash = board.hash();
    if (tacticsCache.find(hash) != tacticsCache.end()) return tacticsCache[hash];

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
    tacticsCache[hash] = score;
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
