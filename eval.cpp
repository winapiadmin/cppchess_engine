#include "eval.h"

// Define enum class for evaluation keys
enum class EvalKey {
    DOUBLED, BACKWARD, BLOCKED, ISLANDED, ISOLATED, DBLISOLATED, WEAK, PAWNRACE,
    SHIELD, STORM, OUTPOST, LEVER, PAWNRAM, OPENPAWN, HOLES,
    UNDEV_KNIGHT, UNDEV_BISHOP, UNDEV_ROOK, DEV_QUEEN,
    OPEN_FILES, SEMI_OPEN_FILES, FIANCHETTO, TRAPPED, KEY_CENTER, 
    SPACE, BADBISHOP, WEAKCOVER, MISSINGPAWN, ATTACK_ENEMY,
    K_OPENING, K_MIDDLE, K_END, PINNED, SKEWERED, DISCOVERED, XRAY,
    FORK, TEMPO_FREEDOM_WEIGHT, TEMPO_OPPONENT_MOBILITY_PENALTY,
    UNDERPROMOTE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN
};
// Struct to hold evaluation weights
struct EvalWeights {
    static const std::unordered_map<EvalKey, int> weights;

    static int getWeight(EvalKey key) {
        auto it = weights.find(key);
        return (it != weights.end()) ? it->second : 0; // Default if key not found
    }
};
// Initialize static weight map
const std::unordered_map<EvalKey, int> EvalWeights::weights = {
    {EvalKey::DOUBLED, 10}, {EvalKey::BACKWARD, 8}, {EvalKey::BLOCKED, 5},
    {EvalKey::ISLANDED, 6}, {EvalKey::ISOLATED, 12}, {EvalKey::DBLISOLATED, 20},
    {EvalKey::WEAK, 15}, {EvalKey::PAWNRACE, 5}, {EvalKey::SHIELD, 10},
    {EvalKey::STORM, 10}, {EvalKey::OUTPOST, 15}, {EvalKey::LEVER, 8},
    {EvalKey::PAWNRAM, 5}, {EvalKey::OPENPAWN, 7}, {EvalKey::HOLES, 10},
    {EvalKey::UNDEV_KNIGHT, 15}, {EvalKey::UNDEV_BISHOP, 10}, {EvalKey::UNDEV_ROOK, 10},
    {EvalKey::DEV_QUEEN, 5}, {EvalKey::OPEN_FILES, 15}, {EvalKey::SEMI_OPEN_FILES, 8},
    {EvalKey::FIANCHETTO, 10}, {EvalKey::TRAPPED, 20}, {EvalKey::KEY_CENTER, 10},
    {EvalKey::SPACE, 12}, {EvalKey::BADBISHOP, 15}, {EvalKey::WEAKCOVER, 18},
    {EvalKey::MISSINGPAWN, 10}, {EvalKey::ATTACK_ENEMY, 20}, {EvalKey::K_OPENING, 25},
    {EvalKey::K_MIDDLE, 20}, {EvalKey::K_END, 10}, {EvalKey::PINNED, 15},
    {EvalKey::SKEWERED, 12}, {EvalKey::DISCOVERED, 14}, {EvalKey::XRAY, 10},
    {EvalKey::FORK, 16}, {EvalKey::TEMPO_FREEDOM_WEIGHT, 8},
    {EvalKey::TEMPO_OPPONENT_MOBILITY_PENALTY, 6}, {EvalKey::UNDERPROMOTE, 5},
    {EvalKey::PAWN, 100}, {EvalKey::KNIGHT, 300}, {EvalKey::BISHOP, 300}, {EvalKey::ROOK, 500}, {EvalKey::QUEEN, 900}
};
std::unordered_map<U64, int> transposition;  // Faster lookup

// Declare only missing functions
int evaluatePawnStructure(const chess::Position&);
int evaluatePieces(const chess::Position&);

// Piece weights for determining game phase
constexpr int PAWN_PHASE    = 0;
constexpr int KNIGHT_PHASE  = 1;
constexpr int BISHOP_PHASE  = 1;
constexpr int ROOK_PHASE    = 2;
constexpr int QUEEN_PHASE   = 4;

// Initial game phase score (sum of all piece values)
constexpr int TOTAL_PHASE = (KNIGHT_PHASE * 4) + (BISHOP_PHASE * 4) +
                            (ROOK_PHASE * 4) + (QUEEN_PHASE * 2);

int phase(const chess::Position& board) {
    int whiteKnights = POPCOUNT64(board.pieces(chess::PieceType::underlying::KNIGHT, chess::Color::underlying::WHITE).getBits());
    int whiteBishops = POPCOUNT64(board.pieces(chess::PieceType::underlying::BISHOP, chess::Color::underlying::WHITE).getBits());
    int whiteRooks = POPCOUNT64(board.pieces(chess::PieceType::underlying::ROOK, chess::Color::underlying::WHITE).getBits());
    int whiteQueens = POPCOUNT64(board.pieces(chess::PieceType::underlying::QUEEN, chess::Color::underlying::WHITE).getBits());

    int blackKnights = POPCOUNT64(board.pieces(chess::PieceType::underlying::KNIGHT, chess::Color::underlying::BLACK).getBits());
    int blackBishops = POPCOUNT64(board.pieces(chess::PieceType::underlying::BISHOP, chess::Color::underlying::BLACK).getBits());
    int blackRooks = POPCOUNT64(board.pieces(chess::PieceType::underlying::ROOK, chess::Color::underlying::BLACK).getBits());
    int blackQueens = POPCOUNT64(board.pieces(chess::PieceType::underlying::QUEEN, chess::Color::underlying::BLACK).getBits());

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

constexpr U64 DARK_SQUARES = 0xAA55AA55AA55AA55ULL; // Dark square mask
constexpr U64 LIGHT_SQUARES = ~DARK_SQUARES; // Light square mask

#define getKingMobility getKingAttacks

// King Safety Zones
constexpr U64 KING_SIDE = FILE_G | FILE_H;
constexpr U64 QUEEN_SIDE = FILE_A | FILE_B;
constexpr U64 CENTER = (FILE_D | FILE_E) & (RANK_4 | RANK_5);

int msb(Bitboard bb)
{
#if __cplusplus >= 202002L
        return std::countl_zero(bits) ^ 63;
#else
#    if defined(__GNUC__)
        return bb ? 63 - __builtin_clzll(bb) : -1;
#    elif defined(_MSC_VER)
        unsigned long idx;
        _BitScanReverse64(&idx, bits);
        return static_cast<int>(idx);
#    else
    // Fallback for compilers without __builtin_clzll and C++20
    if (bb == 0) return -1;
    int index = 0;
    while (bb >>= 1)
    {
        index++;
    }
    return index;
#    endif
#endif
}

// **3. Space Control Evaluation**
int evaluateSpace(U64 ownPawns, bool isWhite) {
    U64 forwardArea = isWhite ? ownPawns & (RANK_4 | RANK_5) : ownPawns & (RANK_5 | RANK_7);
    return EvalWeights::getWeight(EvalKey::SPACE) * countBits(forwardArea); // More space = better position
}

std::vector<Square> scan_reversed(Bitboard bb)
{
    std::vector<Square> iter;
    iter.reserve(64);
    while (bb)
    {
        int square = msb(bb); // Use the optimized msb function
        iter.push_back(square);
        bb &= ~(1ULL << square); // Clear the MSB
    }
    return iter;
}
std::unordered_map<U64, int> bishopCache;

// **1. Evaluate Bad Bishops**
int evaluateBadBishops(const chess::Position& board) {
    U64 hash = board.hash();
    if (bishopCache.find(hash) != bishopCache.end()) return bishopCache[hash];
    U64 myBishops = board.pieces(chess::PieceType::underlying::BISHOP, board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    
    int score = 0;
    while (myBishops) {
        int pos = lsb(myBishops);
        myBishops &= myBishops - 1;
        
        if (countBits(((1ULL<<pos) & DARK_SQUARES) ? (myPawns & DARK_SQUARES) : (myPawns & LIGHT_SQUARES)) > 3) {
            score -= EvalWeights::getWeight(EvalKey::BADBISHOP); // Penalty for bad bishop
        }
    }
    bishopCache[hash] = score;
    return score;
}
// **2. Evaluate King Safety**
int evaluateKingSafety(const chess::Position& board) {
    U64 king = 1ULL << board.kingSq(board.sideToMove()).index();
    U64 ownPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();
    int score=countBits((north(king) | east(king) | west(king)) & ownPawns) * 5;
    
    U64 pawnShield = (king << 8) & ownPawns;
    U64 enemyPressure = (king >> 8) & enemyPieces;

    score -= EvalWeights::getWeight(EvalKey::WEAKCOVER) * countBits(~pawnShield & enemyPressure); // Penalize weak king cover

    // **1. Penalize moving pawns in front of the king**
    U64 pawnsInFront = (int)board.sideToMove() ? 
                       (king << 8) | (king << 16) : 
                       (king >> 8) | (king >> 16);
    int pawnsMissing = 2 - POPCOUNT64(ownPawns & pawnsInFront);
    score -= pawnsMissing * EvalWeights::getWeight(EvalKey::MISSINGPAWN); // Penalty for missing pawns in front of the king
    score -= POPCOUNT64(enemyPieces) * EvalWeights::getWeight(EvalKey::ATTACK_ENEMY); // Penalty for enemy attacks on king
    int gamePhase = phase(board);
    // Adjust king safety weights based on game phase
    switch (gamePhase) {
        case 1: // Opening
            return score * EvalWeights::getWeight(EvalKey::K_OPENING); // Higher weight in the opening
        case 2: // Middlegame
            return score * EvalWeights::getWeight(EvalKey::K_MIDDLE);
        case 3: // Endgame
            return score * EvalWeights::getWeight(EvalKey::K_END); // Lower weight in the endgame
    }
    return score;
}

// **4. Evaluate Rooks on Open/Semi-Open Files**
int evaluateRooksOnFiles(const chess::Position& board) {
    U64 myRooks = board.pieces(chess::PieceType::underlying::ROOK, board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    
    int score = 0;
    while (myRooks) {
        int pos = lsb(myRooks);
        myRooks &= myRooks - 1;
        
        U64 rook = 1ULL << pos;
        int c=0;
        for (int i = 0; i < 8; ++i) {
        	if (rook & FILES[i]) {
            	c += countBits(myPawns & FILES[i]); // No pawns on file = open
        	}
    	}
        if (c==0) score += EvalWeights::getWeight(EvalKey::OPEN_FILES);
        else if (c==1) score += EvalWeights::getWeight(EvalKey::SEMI_OPEN_FILES);
    }
    return score;
}

int evaluateFianchetto(const chess::Position& board) {
    constexpr int fianchettoBishops[2][2] = {{6, 2}, {62, 58}}; // [White][Black] bishops
    constexpr int fianchettoPawns[2][2][2] = {{{5, 7}, {1, 3}}, {{61, 63}, {57, 59}}}; // [White][Black] pawns

    U64 myBishops = board.pieces(chess::PieceType::underlying::BISHOP, board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();

    int score = 0;
    while (myBishops) {
        int pos = lsb(myBishops); // Get least significant bishop
        myBishops &= myBishops - 1; // Remove processed bishop

        // Check if the bishop is in a fianchetto position
        for (int i = 0; i < 2; ++i) {
            if (pos == fianchettoBishops[board.sideToMove()][i]) {
                if ((myPawns & (1ULL << fianchettoPawns[board.sideToMove()][i][0])) &&
                    (myPawns & (1ULL << fianchettoPawns[board.sideToMove()][i][1]))) {
                    score += EvalWeights::getWeight(EvalKey::FIANCHETTO); // Bonus for fianchetto structure
                }
            }
        }
    }
    return score;
}
int evaluateTrappedPieces(const chess::Position& board) {
    U64 myPieces = board.us(board.sideToMove()).getBits();
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    U64 enemyAttacks = board.them(~board.sideToMove()).getBits();
    U64 trapped = 0;

    while (myPieces) {
        int pos = lsb(myPieces); // Get least significant piece
        myPieces &= myPieces - 1; // Remove processed piece

        U64 mask = 1ULL << pos;
        U64 escapeMoves = ((mask & ~0x0101010101010101ULL) << 1) | // Right (avoid wrap)
                          ((mask & ~0x8080808080808080ULL) >> 1) | // Left (avoid wrap)
                          (mask << 8) | (mask >> 8); // Up & Down

        if ((escapeMoves & (myPawns | enemyAttacks))) {
            trapped |= mask;
        }
    }

    return POPCOUNT64(trapped) * EvalWeights::getWeight(EvalKey::TRAPPED);
}


int evaluateKnightForks(const chess::Position& board) {
    U64 myKnights = board.pieces(chess::PieceType::KNIGHT, board.sideToMove()).getBits();
    chess::Bitboard enemyPieces = board.them(board.sideToMove());

    int forkScore = 0;

    // Iterate over each knight
    for (int knightSq : scan_reversed(myKnights)) {
        chess::Bitboard attacks = getKnightAttacks(chess::Square(knightSq));
        int attackedPieces = (attacks & enemyPieces).count();

        if (attackedPieces >= 2) { // Only count forks if at least two enemy pieces are attacked
            forkScore += attackedPieces * EvalWeights::getWeight(EvalKey::FORK);
        }
    }

    return forkScore;
}
// **9. Evaluate Space Control**
int evaluateSpaceControl(const chess::Position& board) {
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    return evaluateSpace(myPawns, board.sideToMove());
}
int evaluateTempo(chess::Position& board) {
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
    return (myMoveCount - forcedMoves) * EvalWeights::getWeight(EvalKey::TEMPO_FREEDOM_WEIGHT) - opponentMoveCount * EvalWeights::getWeight(EvalKey::TEMPO_OPPONENT_MOBILITY_PENALTY);
}
int center_control(const chess::Position& board) {
	const int CENTER_CONTROL_BONUS[64] = {
    	0, 0, 0, 5, 5, 0, 0, 0,  
    	0, 10, 15, 20, 20, 15, 10, 0,
    	0, 15, 30, 40, 40, 30, 15, 0,
    	5, 20, 40, 60, 60, 40, 20, 5,
    	5, 20, 40, 60, 60, 40, 20, 5,
    	0, 15, 30, 40, 40, 30, 15, 0,
    	0, 10, 15, 20, 20, 15, 10, 0,
    	0, 0, 0, 5, 5, 0, 0, 0   
	};

    int score = 0;
    U64 myPieces = board.us(board.sideToMove()).getBits();

    while (myPieces) {
        int sq = lsb(myPieces);  // Get piece square index
        score += CENTER_CONTROL_BONUS[sq];   // Reward based on square
        myPieces &= myPieces - 1;  // Remove LSB
    }
    return score;
}

int key_center_squares_control(const chess::Position& board) {
    int score = 0;
    U64 central_squares = 0x1818000000;
    
    U64 myPieces = board.us(board.sideToMove()).getBits();

    while (myPieces) {
        int sq = lsb(myPieces);
        if (central_squares & (1ULL << sq)) {
            score += EvalWeights::getWeight(EvalKey::KEY_CENTER); // Reward controlling these squares
        }
        myPieces &= myPieces - 1;
    }
    return score;
}
int rook_placement(const chess::Position& board) {
    int bonus = 0;
    U64 rooks = board.pieces(chess::PieceType::ROOK, board.sideToMove()).getBits();
    
    while (rooks) {
        int sq = lsb(rooks);
        U64 file = FILES[sq % 8]; // Get file bitboard
        if ((file & board.occ()) == 0) bonus += EvalWeights::getWeight(EvalKey::OPEN_FILES); // Reward rooks on open files
        rooks &= rooks - 1;
    }
    return bonus;
}
int development(const chess::Position& board) {
    //if (phase(board)==3) return 0; // ✅ Ignore piece development in endgame, removed for blunder cases (e.g. undeveloped pieces in endgame)

    int penalty = 0;

    // **1️⃣ Knights & Bishops Should Not Be at Home**
    U64 undevelopedWhiteKnights = board.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).getBits() & 65;
    U64 undevelopedWhiteBishops = board.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).getBits() & 66;

    U64 undevelopedBlackKnights = board.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK).getBits() & 0x4200000000000000ULL;
    U64 undevelopedBlackBishops = board.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).getBits() & 0x2400000000000000ULL;

    penalty -= EvalWeights::getWeight(EvalKey::UNDEV_KNIGHT) * (POPCOUNT64(undevelopedWhiteKnights) - POPCOUNT64(undevelopedBlackKnights));
    penalty -= EvalWeights::getWeight(EvalKey::UNDEV_BISHOP) * (POPCOUNT64(undevelopedWhiteBishops) - POPCOUNT64(undevelopedBlackBishops));

    // **2️⃣ Rooks Should Not Be Stuck**
    U64 inactiveWhiteRooks = board.pieces(chess::PieceType::ROOK, chess::Color::WHITE).getBits() & 129;
    U64 inactiveBlackRooks = board.pieces(chess::PieceType::ROOK, chess::Color::BLACK).getBits() & 0x8100000000000000ULL;

    penalty -= EvalWeights::getWeight(EvalKey::UNDEV_ROOK) * (POPCOUNT64(inactiveWhiteRooks) - POPCOUNT64(inactiveBlackRooks));

    // **3️⃣ Queens Should Not Come Out Too Early**
    if (phase(board)==1) {  // First 10 moves: early queen penalties
        if (board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE).getBits() & 0xfffffffffffffff7) {
            penalty -= EvalWeights::getWeight(EvalKey::DEV_QUEEN); // Queen moved too early
        }
        if (board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK).getBits() & 0xf7ffffffffffffff) {
            penalty += EvalWeights::getWeight(EvalKey::DEV_QUEEN);
        }
    }

    return penalty;
}

int piece_activity(const chess::Position& board) {
	const int PIECE_ACTIVITY_BONUS[64] = {
    	0, 0, 0, 5, 5, 0, 0, 0,  // Edge ranks penalized
    	0, 10, 15, 20, 20, 15, 10, 0,
    	0, 15, 30, 40, 40, 30, 15, 0,
    	5, 20, 40, 60, 60, 40, 20, 5,
    	5, 20, 40, 60, 60, 40, 20, 5,
    	0, 15, 30, 40, 40, 30, 15, 0,
    	0, 10, 15, 20, 20, 15, 10, 0,
    	0, 0, 0, 5, 5, 0, 0, 0   // Edge ranks penalized
	};

    int score = 0;
    U64 myPieces = board.us(board.sideToMove()).getBits();
    
    while (myPieces) {
        int sq = lsb(myPieces); // Get piece square
        score += PIECE_ACTIVITY_BONUS[sq]; // Reward centralization
        myPieces &= myPieces - 1; // Remove LSB
    }
    return score;
}
int evaluatePieceActivity(const chess::Position& board) {
    int score = 0;
    U64 myPieces = board.us(board.sideToMove()).getBits();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();

    // Reward active pieces (knights, bishops, rooks, queens)
    while (myPieces) {
        int sq = lsb(myPieces);
        U64 attacks = chess::attacks::attackers(board, board.sideToMove(), chess::Square(sq)).getBits();

        // Bonus for attacking enemy pieces
        score += POPCOUNT64(attacks & enemyPieces) * EvalWeights::getWeight(EvalKey::ATTACK_ENEMY);

        myPieces &= myPieces - 1; // Remove LSB
    }

    return score;
}
// **1. Back Rank Mate Detection**
inline bool isBackRankMate(U64 king, U64 enemyRooks, U64 enemyQueens, bool isWhite) {
    U64 backRank = isWhite ? RANK_1 : RANK_8;
    return (king & backRank) && ((enemyRooks | enemyQueens) & backRank);
}

// **2. Skewer Detection**
inline bool isSkewer(U64 attacker, U64 target, U64 behindTarget) {
    return (attacker & target) && (behindTarget & target);
}
/*
// **3. Discovered Attack/Check**
inline bool isDiscoveredAttack(U64 movingPiece, U64 attacker, U64 target) {
    U64 afterMove = movingPiece & ~attacker; // Simulate moving the piece
    return (afterMove & target); // If the line opens, it's a discovered attack
}*/

// **4. X-Ray Attack Detection**
inline bool isXRayAttack(U64 attacker, U64 blocker, U64 target) {
    return (attacker & blocker) && (attacker & target);
}
inline bool isTrappedPiece(const chess::Position& board, chess::Square pieceSq) {
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
    			int sq = lsb(knightAttacks);  // Extract LSB (fast)
    			p=board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false; // The knight has a safe escape
                }
    			knightAttacks &= knightAttacks - 1;  // Remove LSB
			}
            return true;

        case chess::BISHOP:
			while (bishopAttacks) {
    			int sq = lsb(bishopAttacks);  // Extract LSB (fast)
    			p=board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false;
                }
    			bishopAttacks &= bishopAttacks - 1;  // Remove LSB
			}
            return true;

        case chess::ROOK:
			while (rookAttacks) {
    			int sq = lsb(rookAttacks);  // Extract LSB (fast)
    			p=board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false;
                }
    			rookAttacks &= rookAttacks - 1;  // Remove LSB
            }
            return true;

        case chess::QUEEN:
			while (queenAttacks) {
    			int sq = lsb(queenAttacks);  // Extract LSB (fast)
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


inline bool isPinned(chess::Position& board, chess::Square pieceSq) {
    chess::Color side = board.at(pieceSq).color();
    chess::Square kingSq = board.kingSq(side);

    board.makeNullMove();
    bool isPinned = !chess::attacks::attackers(board, ~side, kingSq).empty();
    board.unmakeNullMove();

    return isPinned;
}
std::unordered_map<U64, int> tacticsCache;
int evaluateTactics(const chess::Position& board) {
    U64 hash = board.hash();
    if (tacticsCache.find(hash) != tacticsCache.end()) return tacticsCache[hash];

    int score = 0;
    U64 myPieces = board.us(board.sideToMove()).getBits();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();

    // **1. Pin Check**
    U64 pinnedPieces = myPieces; // Cache the bitboard
    while (pinnedPieces) {
        int pieceSq = lsb(pinnedPieces);
        pinnedPieces &= pinnedPieces - 1;
        if (isPinned(const_cast<chess::Position&>(board), chess::Square(pieceSq))) {
            score -= EvalWeights::getWeight(EvalKey::PINNED);
        }
    }

    // **2. Skewer Check** (Optimized)
    U64 skewerAttackers = board.pieces(chess::PieceType::ROOK, board.sideToMove()).getBits() |
                          board.pieces(chess::PieceType::QUEEN, board.sideToMove()).getBits();
    while (skewerAttackers) {
        int attackerSq = lsb(skewerAttackers);
        skewerAttackers &= skewerAttackers - 1;

        U64 targetPieces = enemyPieces;  // Cache enemy bitboard
        while (targetPieces) {
            int targetSq = lsb(targetPieces);
            targetPieces &= targetPieces - 1;

            U64 behindTarget = enemyPieces;
            while (behindTarget) {
                int behindTargetSq = lsb(behindTarget);
                behindTarget &= behindTarget - 1;

                if (isSkewer(1ULL << attackerSq, 1ULL << targetSq, 1ULL << behindTargetSq)) {
                    score += EvalWeights::getWeight(EvalKey::SKEWERED);
                }
            }
        }
    }
/*
    // **3. Discovered Attack Check (Optimized)**
    U64 mySlidingPieces = board.pieces(chess::PieceType::ROOK, board.sideToMove()).getBits() |
                          board.pieces(chess::PieceType::BISHOP, board.sideToMove()).getBits() |
                          board.pieces(chess::PieceType::QUEEN, board.sideToMove()).getBits();
    while (mySlidingPieces) {
        int attackerSq = lsb(mySlidingPieces);
        mySlidingPieces &= mySlidingPieces - 1;

        U64 targets = enemyPieces;
        while (targets) {
            int targetSq = lsb(targets);
            targets &= targets - 1;

            if (isDiscoveredAttack(1ULL << attackerSq, 1ULL << attackerSq, 1ULL << targetSq)) {
                score += EvalWeights::getWeight(EvalKey::DISCOVERED);
            }
        }
    }
*/
    // **4. X-Ray Attack Check**
    U64 xRayPieces = board.pieces(chess::PieceType::ROOK, board.sideToMove()).getBits() |
                     board.pieces(chess::PieceType::QUEEN, board.sideToMove()).getBits();
    while (xRayPieces) {
        int pieceSq = lsb(xRayPieces);
        xRayPieces &= xRayPieces - 1;

        U64 enemyTargets = enemyPieces;
        while (enemyTargets) {
            int enemyPieceSq = lsb(enemyTargets);
            enemyTargets &= enemyTargets - 1;

            if (isXRayAttack(1ULL << pieceSq, 1ULL << enemyPieceSq, 1ULL << board.kingSq(~board.sideToMove()).index())) {
                score += EvalWeights::getWeight(EvalKey::XRAY);
            }
        }
    }

    tacticsCache[hash] = score;
    return score;
}
#define LIGHT_SQUARES  0x55aa55aa55aa55aaULL
#define DARK_SQUARES   0xaa55aa55aa55aa55ULL

// **1. Doubled Pawns Penalty**
int doubled(U64 whitePawns, U64 blackPawns) {
    int score = 0;
    for (int i = 0; i < 8; ++i) {
        int whiteCount = POPCOUNT64(whitePawns & FILES[i]);
        int blackCount = POPCOUNT64(blackPawns & FILES[i]);

        if (whiteCount > 1) score -= (whiteCount - 1) * EvalWeights::getWeight(EvalKey::DOUBLED);
        if (blackCount > 1) score += (blackCount - 1) * EvalWeights::getWeight(EvalKey::DOUBLED);
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
            int pos = lsb(whiteFile);
            whiteFile &= whiteFile - 1;
            if (!(whiteAdjacent & (1ULL << pos)) && (blackPawns & (fileMask >> 8))) score -= EvalWeights::getWeight(EvalKey::BACKWARD);
        }
        while (blackFile) {
            int pos = lsb(blackFile);
            blackFile &= blackFile - 1;
            if (!(blackAdjacent & (1ULL << pos)) && (whitePawns & (fileMask << 8))) score += EvalWeights::getWeight(EvalKey::BACKWARD);
        }
    }
    return score;
}

// **3. Pawn Blockage**
int blockage(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();
    return POPCOUNT64((board.sideToMove() ? north(pawns) : south(pawns)) & enemyPieces) * EvalWeights::getWeight(EvalKey::BLOCKED);
}

// **4. Pawn Islands**
int pawnIslands(const chess::Position& board) {
    int islands = 0;
    U64 remaining = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    for (int i = 0; i < 8; ++i) {
        if (remaining & FILES[i]) {
            islands++;
            remaining &= ~FILES[i];
        }
    }
    return islands * EvalWeights::getWeight(EvalKey::ISLANDED);
}

// **5. Isolated & Doubly Isolated Pawns**
int isolated(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    return POPCOUNT64(pawns & ~(west(pawns) | east(pawns))) * EvalWeights::getWeight(EvalKey::ISOLATED);
}
int dblisolated(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 isolated = pawns & ~(west(pawns) | east(pawns));
    return POPCOUNT64(isolated & (isolated - 1)) * EvalWeights::getWeight(EvalKey::DBLISOLATED);
}

// **6. Weak Pawns**
int weaks(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    U64 weakPawns = (pawns & ~(west(pawns) | east(pawns))) | ((north(pawns) & enemyPawns) | (south(pawns) & enemyPawns));
    return POPCOUNT64(weakPawns) * EvalWeights::getWeight(EvalKey::WEAK);
}

// **7. Pawn Race**
int pawnRace(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    int minDistance = 8;
    while (pawns) {
        int pos = lsb(pawns);
        pawns &= pawns - 1;
        int rank = pos / 8;
        minDistance = std::min(minDistance, board.sideToMove() ? (7 - rank) : rank);
    }
    return (phase(board) == 3) ? (8 - minDistance) * EvalWeights::getWeight(EvalKey::PAWNRACE) : 8 - minDistance;
}

// **8. Color Weakness**
int weakness(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    return POPCOUNT64(~((pawns & DARK_SQUARES) | (pawns & LIGHT_SQUARES))) * EvalWeights::getWeight(EvalKey::WEAK);
}

// **9. Pawn Shield**
int pawnShield(const chess::Position& board) {
    U64 kingBB = 1ULL << board.kingSq(board.sideToMove()).index();
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    return POPCOUNT64(board.sideToMove() ? north(kingBB) & pawns : south(kingBB) & pawns) * EvalWeights::getWeight(EvalKey::SHIELD);
}

// **10. Pawn Storm**
int pawnStorm(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 storm = board.sideToMove() ? (north(pawns) | north(north(pawns))) : (south(pawns) | south(south(pawns)));
    return POPCOUNT64(storm) * EvalWeights::getWeight(EvalKey::STORM);
}

// **11. Outposts**
int outpost(const chess::Position& board) {
    U64 knights = board.pieces(chess::PieceType::KNIGHT, board.sideToMove()).getBits();
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 support = board.sideToMove() ? (south(knights) & pawns) : (north(knights) & pawns);
    return POPCOUNT64(support & ~(north(pawns) | south(pawns))) * EvalWeights::getWeight(EvalKey::OUTPOST);
}

// **12. Pawn Levers**
int pawnLevers(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    return POPCOUNT64((east(pawns) & enemyPawns) | (west(pawns) & enemyPawns)) * EvalWeights::getWeight(EvalKey::LEVER);
}

// **13. Pawn Rams**
int evaluatePawnRams(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    return POPCOUNT64((north(pawns) & south(enemyPawns)) | (south(pawns) & north(enemyPawns))) * EvalWeights::getWeight(EvalKey::PAWNRAM);
}

// **1. Unfree Pawns (Blocked Pawns)**
int evaluateUnfreePawns(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    U64 blocked = (north(pawns) & enemyPawns) | (south(pawns) & enemyPawns);
    return POPCOUNT64(blocked) * EvalWeights::getWeight(EvalKey::BLOCKED); // Higher penalty for blocked pawns
}

// **2. Open Pawns (Pawns with no obstacles in front)**
int evaluateOpenPawns(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();
    U64 openFiles = pawns & ~(north(pawns) & enemyPawns);
    return POPCOUNT64(openFiles) * EvalWeights::getWeight(EvalKey::OPENPAWN); // Slight reward for open files
}

// **3. Holes (Uncontested squares that cannot be controlled by pawns)**
int holes(const chess::Position& board) {
    U64 whitePawns = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE).getBits();
    U64 blackPawns = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK).getBits();
    U64 pawnControl = north(whitePawns) | south(blackPawns);
    return POPCOUNT64(~pawnControl) * EvalWeights::getWeight(EvalKey::HOLES); // Penalize holes in structure
}

int underpromote(chess::Position &board) {
    if (board.move_stack.empty()) return 0; // No move history → no underpromotion check

    const chess::Move &lastMove = board.move_stack.back(); // Get last move
    if (lastMove.typeOf() != chess::Move::PROMOTION) return 0; // Skip non-promotions

    // Undo move temporarily to fetch piece before promotion
    board.pop();
    const chess::Square from = lastMove.from();
    const chess::Square to = lastMove.to();
    const chess::Piece movedPiece = board.at(from);
    board.makeMove(lastMove);

    // Ensure it was a pawn move
    if (!(movedPiece == chess::Piece::WHITEPAWN || movedPiece == chess::Piece::BLACKPAWN)) return 0;

    // Precompute necessary board properties
    U64 enemyKingBB = 1ULL << board.kingSq(~board.sideToMove()).index();
    U64 enemyPieces = board.them(board.sideToMove()).getBits();
    U64 friendlyPieces = board.us(board.sideToMove()).getBits();
    bool isWhite = (board.sideToMove() == chess::Color::WHITE);
    int gamePhase = phase(board);

    // **1. Stalemate Prevention**
    U64 kingMobility = getKingMobility(lsb(enemyKingBB), enemyPieces | friendlyPieces);
    if (kingMobility == 0) return EvalWeights::getWeight(EvalKey::UNDERPROMOTE); // Stalemate risk → prefer underpromotion

    // **2. Tactical Fork or Mating Net Check (Prefer Knight underpromotion)**
    if (chess::Square::back_rank(to, chess::Color(isWhite))) {
        if (getKnightAttacks(to) & enemyKingBB) return EvalWeights::getWeight(EvalKey::UNDERPROMOTE); // Knight promotion is better
    }

    // **3. Underpromotion in the Opening (Forcing Move Continuation)**
    if (gamePhase == 1 && (1ULL << to.index()) & enemyPieces) return EvalWeights::getWeight(EvalKey::UNDERPROMOTE);

    // **4. Endgame Considerations (Avoid Losing Promotion)**
    if (gamePhase == 3) {
        // Immediate Fork or Skewer Check
        U64 queenBB = 1ULL << to.index();
        if (getKnightAttacks(to) & enemyPieces) return EvalWeights::getWeight(EvalKey::UNDERPROMOTE); // If knight forks queen, avoid queen promotion
        if (isXRayAttack(queenBB, enemyPieces, enemyKingBB)) return EvalWeights::getWeight(EvalKey::UNDERPROMOTE); // If queen is X-Rayed or skewered
    }

    // **5. Avoiding Forks (Critical Defensive Underpromotion)**
    if (enemyPieces & getKnightAttacks(to)) return EvalWeights::getWeight(EvalKey::UNDERPROMOTE); // Underpromotion avoids material loss

    // If it was an underpromotion but not necessary, penalize it
    return (lastMove.promotionType().internal() != chess::PieceType::QUEEN) ? 0 : EvalWeights::getWeight(EvalKey::UNDERPROMOTE);
}

// PAWN
const int PAWN_PSQT[2][64] = {
    { // Opening
         0,   0,   0,   0,   0,   0,  0,  0,
        50,  50,  50,  50,  50,  50, 50, 50,
        10,  10,  20,  30,  30,  20, 10, 10,
         5,   5,  10,  25,  25,  10,  5,  5,
         0,   0,   0,  20,  20,   0,  0,  0,
         5,  -5, -10,   0,   0, -10, -5,  5,
         5,  10,  10, -20, -20,  10, 10,  5,
         0,   0,   0,   0,   0,   0,  0,  0
    },
    { // Endgame
         0,   0,   0,   0,   0,   0,  0,  0,
        50,  50,  50,  50,  50,  50, 50, 50,
        10,  10,  20,  30,  30,  20, 10, 10,
         5,   5,  10,  25,  25,  10,  5,  5,
         0,   0,   0,  20,  20,   0,  0,  0,
         5,  -5, -10,   0,   0, -10, -5,  5,
         5,  10,  10, -20, -20,  10, 10,  5,
         0,   0,   0,   0,   0,   0,  0,  0
    }
};

// KNIGHT
const int KNIGHT_PSQT[2][64] = {
    { // Opening
        -50, -40, -30, -30, -30, -30, -40, -50,
        -40, -20,   0,   0,   0,   0, -20, -40,
        -30,   0,  10,  15,  15,  10,   0, -30,
        -30,   5,  15,  20,  20,  15,   5, -30,
        -30,   0,  15,  20,  20,  15,   0, -30,
        -30,   5,  10,  15,  15,  10,   5, -30,
        -40, -20,   0,   5,   5,   0, -20, -40,
        -50, -40, -30, -30, -30, -30, -40, -50
    },
    { // Endgame
        -50, -40, -30, -30, -30, -30, -40, -50,
        -40, -20,   0,   0,   0,   0, -20, -40,
        -30,   0,  10,  15,  15,  10,   0, -30,
        -30,   5,  15,  20,  20,  15,   5, -30,
        -30,   0,  15,  20,  20,  15,   0, -30,
        -30,   5,  10,  15,  15,  10,   5, -30,
        -40, -20,   0,   5,   5,   0, -20, -40,
        -50, -40, -30, -30, -30, -30, -40, -50
    }
};

// BISHOP
const int BISHOP_PSQT[2][64] = {
    { // Opening
        -20, -10, -10, -10, -10, -10, -10, -20,
        -10,   5,   0,   0,   0,   0,   5, -10,
        -10,  10,  10,  10,  10,  10,  10, -10,
        -10,   0,  10,  10,  10,  10,   0, -10,
        -10,   5,   5,  10,  10,   5,   5, -10,
        -10,   0,   5,  10,  10,   5,   0, -10,
        -10,   0,   0,   0,   0,   0,   0, -10,
        -20, -10, -10, -10, -10, -10, -10, -20
    },
    { // Endgame
        -20, -10, -10, -10, -10, -10, -10, -20,
        -10,   5,   0,   0,   0,   0,   5, -10,
        -10,  10,  10,  10,  10,  10,  10, -10,
        -10,   0,  10,  10,  10,  10,   0, -10,
        -10,   5,   5,  10,  10,   5,   5, -10,
        -10,   0,   5,  10,  10,   5,   0, -10,
        -10,   0,   0,   0,   0,   0,   0, -10,
        -20, -10, -10, -10, -10, -10, -10, -20
    }
};

// ROOK
const int ROOK_PSQT[2][64] = {
    { // Opening
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
        25,  25,  25,  25,  25,  25,  25,  25,
         0,   0,   0,   5,   5,   0,   0,   0
    },
    { // Endgame
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
         0,   0,   5,  10,  10,   5,   0,   0,
        25,  25,  25,  25,  25,  25,  25,  25,
         0,   0,   0,   5,   5,   0,   0,   0
    }
};

const int QUEEN_PSQT[2][64] = {
    { // Opening
        -20, -10, -10,  -5,  -5, -10, -10, -20,
        -10,   0,   5,   0,   0,   5,   0, -10,
        -10,   5,   5,   5,   5,   5,   5, -10,
         -5,   0,   5,   5,   5,   5,   0,  -5,
         -5,   0,   5,   5,   5,   5,   0,  -5,
        -10,   5,   5,   5,   5,   5,   5, -10,
        -10,   0,   5,   0,   0,   5,   0, -10,
        -20, -10, -10,  -5,  -5, -10, -10, -20
    },
    { // Endgame
        -10,  -5,  -5,   0,   0,  -5,  -5, -10,
         -5,   0,   0,   5,   5,   0,   0,  -5,
         -5,   0,   5,   5,   5,   5,   0,  -5,
          0,   5,   5,   5,   5,   5,   5,   0,
          0,   5,   5,   5,   5,   5,   5,   0,
         -5,   0,   5,   5,   5,   5,   0,  -5,
         -5,   0,   0,   5,   5,   0,   0,  -5,
        -10,  -5,  -5,   0,   0,  -5,  -5, -10
    }
};

// KING (simplified for space)
const int KING_PSQT[2][64] = {
    { // Opening
        -30, -40, -40, -50, -50, -40, -40, -30,
        -30, -40, -40, -50, -50, -40, -40, -30,
        -30, -40, -40, -50, -50, -40, -40, -30,
        -30, -40, -40, -50, -50, -40, -40, -30,
        -20, -30, -30, -40, -40, -30, -30, -20,
        -10, -20, -20, -20, -20, -20, -20, -10,
         20,  20,   0,   0,   0,   0,  20,  20,
         20,  30,  10,   0,   0,  10,  30,  20
    },
    { // Endgame
        -50, -40, -30, -20, -20, -30, -40, -50,
        -30, -20, -10,   0,   0, -10, -20, -30,
        -30, -10,  20,  30,  30,  20, -10, -30,
        -30, -10,  30,  40,  40,  30, -10, -30,
        -30, -10,  30,  40,  40,  30, -10, -30,
        -30, -10,  20,  30,  30,  20, -10, -30,
        -30, -30,   0,   0,   0,   0, -30, -30,
        -50, -30, -30, -30, -30, -30, -30, -50
    }
};
int psqt_eval(const chess::Position& board) {
    int score = 0;
    int phaseIndex = (phase(board) == 3) ? 1 : 0; // 0 = Opening, 1 = Endgame
    chess::Color side = board.sideToMove();
    
    U64 pieces = board.us(side).getBits();
    
    while (pieces) {
        int sq = lsb(pieces);
        chess::Piece piece = board.at(sq);
        // Flip square for Black (mirror vertically)
        int mirrorSq = (side == chess::Color::WHITE) ? sq : (sq ^ 56);

        switch (piece.type().internal()) {
            case chess::PieceType::underlying::PAWN:
                score += PAWN_PSQT[phaseIndex][mirrorSq];
                break;
            case chess::PieceType::underlying::KNIGHT:
                score += KNIGHT_PSQT[phaseIndex][mirrorSq];
                break;
            case chess::PieceType::underlying::BISHOP:
                score += BISHOP_PSQT[phaseIndex][mirrorSq];
                break;
            case chess::PieceType::underlying::ROOK:
                score += ROOK_PSQT[phaseIndex][mirrorSq];
                break;
            case chess::PieceType::underlying::QUEEN:
                score += QUEEN_PSQT[phaseIndex][mirrorSq];
                break;
            case chess::PieceType::underlying::KING:
                score += KING_PSQT[phaseIndex][mirrorSq];
                break;
            default:
                break;
        }

        pieces &= pieces - 1; // Remove LSB
    }
    return score; // Negate for Black
}

int eval(chess::Position board) {
    U64 hash = board.hash();
    // Faster lookup & insertion
    auto [it, inserted] = transposition.emplace(hash, 0);
    if (!inserted) return it->second;  // Already exists, return stored evaluation

    if (board.inCheck() && board.isGameOver().first != chess::GameResultReason::NONE) return MAX;
    if (board.isGameOver().first != chess::GameResultReason::NONE) return 0;

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

    // Compute material score
    int eval = (EvalWeights::getWeight(EvalKey::PAWN) * (pieceCount[0] - pieceCount[5])) +
               (EvalWeights::getWeight(EvalKey::KNIGHT) * (pieceCount[1] - pieceCount[6])) +
               (EvalWeights::getWeight(EvalKey::BISHOP) * (pieceCount[2] - pieceCount[7])) +
               (EvalWeights::getWeight(EvalKey::ROOK) * (pieceCount[3] - pieceCount[8])) +
               (EvalWeights::getWeight(EvalKey::QUEEN) * (pieceCount[4] - pieceCount[9]));
    
    // Evaluate positional aspects
    eval += evaluatePawnStructure(board);
    eval += evaluatePieces(board);
    eval += evaluateKingSafety(board);
    eval += evaluateTactics(board);
    eval += piece_activity(board);
    eval += development(board);
    eval += rook_placement(board);
    eval += center_control(board);
    eval += key_center_squares_control(board);
    eval += psqt_eval(board);
    eval += evaluatePieceActivity(board);
    board.makeNullMove();
    // Evaluate positional aspects
    eval -= evaluatePawnStructure(board);
    eval -= evaluatePieces(board);
    eval -= evaluateKingSafety(board);
    eval -= evaluateTactics(board);
    eval -= piece_activity(board);
    eval -= development(board);
    eval -= rook_placement(board);
    eval -= center_control(board);
    eval -= key_center_squares_control(board);
    eval -= psqt_eval(board);
    eval -= evaluatePieceActivity(board);
    board.unmakeNullMove();
    
    it->second = eval;  // Store result
    return eval;
}

// Use `const chess::Position&` to avoid copies
int evaluatePawnStructure(const chess::Position& board) {
    chess::Position b=board;
    return -(isolated(board) + dblisolated(board) + weaks(board) + blockage(board) +
             holes(board) + underpromote(b) + weakness(board) + evaluatePawnRams(board)) +
           (pawnIslands(board) + pawnRace(board) + pawnShield(board) + pawnStorm(board) +
            pawnLevers(board) + outpost(board) + evaluateUnfreePawns(board) + evaluateOpenPawns(board));
}

// Use `const chess::Position&` to avoid copies
int evaluatePieces(const chess::Position& board) {
    return -(evaluateBadBishops(board) + evaluateTrappedPieces(board) + evaluateKnightForks(board)) +
           (evaluateFianchetto(board) + evaluateRooksOnFiles(board));
}

int piece_value(chess::PieceType piece) {
    switch (piece) {
        case (int)chess::PieceType::PAWN:   return EvalWeights::getWeight(EvalKey::PAWN);
        case (int)chess::PieceType::KNIGHT: return EvalWeights::getWeight(EvalKey::KNIGHT);
        case (int)chess::PieceType::BISHOP: return EvalWeights::getWeight(EvalKey::BISHOP);
        case (int)chess::PieceType::ROOK:   return EvalWeights::getWeight(EvalKey::ROOK);
        case (int)chess::PieceType::QUEEN:  return EvalWeights::getWeight(EvalKey::QUEEN);
        default: return 0; // King has no standard value
    }
}
