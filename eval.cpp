#include "eval.h"

// Define enum class for evaluation keys
enum class EvalKey {
    DOUBLED, BACKWARD, BLOCKED, ISLANDED, ISOLATED, DBLISOLATED, WEAK, PAWNRACE,
    SHIELD, STORM, OUTPOST, LEVER, PAWNRAM, OPENPAWN, HOLES,
    UNDEV_KNIGHT, UNDEV_BISHOP, UNDEV_ROOK, DEV_QUEEN, OPEN_FILES,
    SEMI_OPEN_FILES, FIANCHETTO, TRAPPED, KEY_CENTER, SPACE,
    BADBISHOP, WEAKCOVER, MISSINGPAWN, ATTACK_ENEMY, K_OPENING,
    K_MIDDLE, K_END, PINNED, SKEWERED, DISCOVERED, FORK,
    TEMPO_FREEDOM_WEIGHT, TEMPO_OPPONENT_MOBILITY_PENALTY,
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
    {EvalKey::SKEWERED, 12}, {EvalKey::DISCOVERED, 14}, {EvalKey::FORK, 16},
    {EvalKey::TEMPO_FREEDOM_WEIGHT, 8}, {EvalKey::TEMPO_OPPONENT_MOBILITY_PENALTY, 6},
    {EvalKey::UNDERPROMOTE, 5}, {EvalKey::PAWN, 100}, {EvalKey::KNIGHT, 300},
    {EvalKey::BISHOP, 300}, {EvalKey::ROOK, 500}, {EvalKey::QUEEN, 900}
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

std::vector<Square> scan_reversed(chess::Bitboard bb)
{
    std::vector<Square> iter;
    iter.reserve(64);
    while (bb)
    {
        int square = bb.pop(); // Use the optimized msb function
        iter.push_back(square);
    }
    return iter;
}
// **1. Evaluate Bad Bishops**
int evaluateBadBishops(const chess::Position& board) {
    auto myBishops = board.pieces(chess::PieceType::underlying::BISHOP, board.sideToMove());
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    
    int score = 0;
    while (myBishops) {
        chess::Square pos = myBishops.pop();
        
        if (countBits(pos.is_dark() ? (myPawns & DARK_SQUARES) : (myPawns & LIGHT_SQUARES)) > 3) {
            score -= EvalWeights::getWeight(EvalKey::BADBISHOP); // Penalty for bad bishop
        }
    }
    return score;
}
int evaluateKingSafety(const chess::Position& board) {
    // 1) One-time fetches
    const int side = board.sideToMove();
    const int ksq  = board.kingSq(side).index();
    const U64 king = 1ULL << ksq;

    U64 ownPawns    = board.pieces(chess::PieceType::underlying::PAWN, side).getBits();
    U64 enemyPieces = board.them(side).getBits();

    // 2) Preload all weights into locals
    constexpr int COVER_BONUS   = 5;
    const int wWeakCover      = EvalWeights::getWeight(EvalKey::WEAKCOVER);
    const int wMissingPawn    = EvalWeights::getWeight(EvalKey::MISSINGPAWN);
    const int wAttackEnemy    = EvalWeights::getWeight(EvalKey::ATTACK_ENEMY);
    static const int phaseWeight[4] = {
        0,
        EvalWeights::getWeight(EvalKey::K_OPENING),
        EvalWeights::getWeight(EvalKey::K_MIDDLE),
        EvalWeights::getWeight(EvalKey::K_END),
    };

    // 3) Adjacent pawn cover: north, east, west
    U64 maskN = king << 8;
    U64 maskE = (king & ~FILE_H) >> 1;
    U64 maskW = (king & ~FILE_A) << 1;
    U64 coverMask = maskN | maskE | maskW;
    int score = POPCOUNT64(coverMask & ownPawns) * COVER_BONUS;

    // 4) Weak cover: enemy pressure on the square in front of king without pawn shield
    U64 frontShield = (side ? (king << 8) : (king >> 8)) & ownPawns;
    U64 frontAttack = (side ? (king >> 8) : (king << 8)) & enemyPieces;
    U64 weakMask    = frontAttack & ~frontShield;
    score -= wWeakCover * POPCOUNT64(weakMask);

    // 5) Missing pawns in front (one and two ranks ahead)
    U64 f1 = (side ? king << 8  : king >> 8);
    U64 f2 = (side ? king << 16 : king >> 16);
    U64 inFront = f1 | f2;
    int present = POPCOUNT64(ownPawns & inFront);
    score -= (2 - present) * wMissingPawn;

    // 6) Penalize total enemy attacks
    score -= POPCOUNT64(enemyPieces) * wAttackEnemy;

    // 7) Phase adjustment
    int p = phase(board);  // 1=open, 2=mid, 3=end
    return score * phaseWeight[p];
}


// **4. Evaluate Rooks on Open/Semi-Open Files**
int evaluateRooksOnFiles(const chess::Position& board) {
    auto myRooks = board.pieces(chess::PieceType::underlying::ROOK, board.sideToMove());
    U64 myPawns = board.pieces(chess::PieceType::underlying::PAWN, board.sideToMove()).getBits();
    
    int score = 0;
    while (myRooks) {
        int pos=myRooks.pop();
        int c=POPCOUNT64(myPawns & FILES[pos&7]); // No pawns on file = open
        if (c==0) score += EvalWeights::getWeight(EvalKey::OPEN_FILES);
        else if (c==1) score += EvalWeights::getWeight(EvalKey::SEMI_OPEN_FILES);
    }
    return score;
}
constexpr uint64_t shiftLeft(int position) {
    return 1ULL << position;
}

int evaluateFianchetto(const chess::Position& board) {
    // Define positions for bishops and pawns based on side to move
    constexpr int fianchettoBishops[2][2] = {{6, 2}, {62, 58}}; // [White][Black] bishops
    constexpr int fianchettoPawns[2][2][2] = {{{5, 7}, {1, 3}}, {{61, 63}, {57, 59}}}; // [White][Black] pawns

    // Get the current side to move
    int side = board.sideToMove();

    // Precompute the left-shifted bit positions for the pawns at fianchetto positions at compile-time
    constexpr uint64_t fianchettoPawnMask1 = shiftLeft(fianchettoPawns[0][0][0]) | shiftLeft(fianchettoPawns[0][0][1]);
    constexpr uint64_t fianchettoPawnMask2 = shiftLeft(fianchettoPawns[0][1][0]) | shiftLeft(fianchettoPawns[0][1][1]);
    constexpr uint64_t fianchettoPawnMask3 = shiftLeft(fianchettoPawns[1][0][0]) | shiftLeft(fianchettoPawns[1][0][1]);
    constexpr uint64_t fianchettoPawnMask4 = shiftLeft(fianchettoPawns[1][1][0]) | shiftLeft(fianchettoPawns[1][1][1]);

    // Combine the two masks into a single mask for more efficient checking
    constexpr uint64_t combinedFianchettoPawnMaskWhite = fianchettoPawnMask1 | fianchettoPawnMask2;
    constexpr uint64_t combinedFianchettoPawnMaskBlack = fianchettoPawnMask3 | fianchettoPawnMask4;

    // Get the positions of bishops for the current side to move
    auto myBishops = board.pieces(chess::PieceType::underlying::BISHOP, side);
    auto myPawns = board.pieces(chess::PieceType::underlying::PAWN, side);

    // Precompute the bishop positions for the current side to move
    int bishopPos1 = fianchettoBishops[side][0];
    int bishopPos2 = fianchettoBishops[side][1];

    int score = 0;

    // Iterate through all the bishops
    while (myBishops) {
        int pos = myBishops.pop();

        // Check if the bishop is in a fianchetto position
        if (pos == bishopPos1 || pos == bishopPos2) {
            // Check if the pawns are in the corresponding fianchetto positions
            if ((side == 0 && (myPawns & combinedFianchettoPawnMaskWhite)) || 
                (side == 1 && (myPawns & combinedFianchettoPawnMaskBlack))) {
                score += EvalWeights::getWeight(EvalKey::FIANCHETTO); // Bonus for fianchetto structure
            }
        }
    }

    return score;
}
// precompute, at compile-time, for each square the bitboard of N,S,E,W neighbors
static constexpr U64 makeNeighbors(int sq) {
    U64 m = 0;
    int file = sq & 7;
    if (file > 0)        m |= 1ULL << (sq - 1);  // W
    if (file < 7)        m |= 1ULL << (sq + 1);  // E
    if (sq >= 8)         m |= 1ULL << (sq - 8);  // N
    if (sq < 56)         m |= 1ULL << (sq + 8);  // S
    return m;
}
static constexpr std::array<U64,64> orthogonal = [](){
    std::array<U64,64> a = {};
    for(int i = 0; i < 64; ++i) a[i] = makeNeighbors(i);
    return a;
}();
int evaluateTrappedPieces(const chess::Position& board) {
    int side       = board.sideToMove();
    auto myPieces  = board.us(side);
    U64 myPawns    = board.pieces(chess::PieceType::underlying::PAWN, side).getBits();
    U64 enemyAttks = board.them(side).getBits();

    // Precompute once:
    U64 blockers   = myPawns | enemyAttks;
    U64 freeSquares = ~blockers;  

    int trapped = 0;
    while (myPieces) {
        int sq = myPieces.pop();  
        // only one AND and one compare-to-zero per piece
        if ((orthogonal[sq] & freeSquares) == 0) {
            ++trapped;
        }
    }

    return trapped * EvalWeights::getWeight(EvalKey::TRAPPED);
}

int evaluateKnightForks(const chess::Position& board) {
    chess::Bitboard myKnights = board.pieces(chess::PieceType::KNIGHT, board.sideToMove());
    chess::Bitboard enemyPieces = board.them(board.sideToMove());

    int forkScore = 0;

    // Iterate over each knight
    for (int knightSq : scan_reversed(myKnights)) {
        chess::Bitboard attacks = getKnightAttacks(chess::Square(knightSq));
        const int attackedPieces = (attacks & enemyPieces).count();

        if (attackedPieces >= 2) { // Only count forks if at least two enemy pieces are attacked
            forkScore += attackedPieces * EvalWeights::getWeight(EvalKey::FORK);
        }
    }

    return forkScore;
}
// Precompute the forward‐area masks at compile time
static constexpr U64 SPACE_MASKS[2] = {
    RANK_4 | RANK_5,  // White’s forward area
    RANK_5 | RANK_7   // Black’s forward area
};

int evaluateSpaceControl(const chess::Position& board) {
    const int side    = board.sideToMove();
    const U64 pawns   = board.pieces(chess::PieceType::underlying::PAWN, side).getBits();
    const U64 mask    = SPACE_MASKS[side];
    const int weight  = EvalWeights::getWeight(EvalKey::SPACE);

    // single AND + popcount + multiply
    return weight * POPCOUNT64(pawns & mask);
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
    auto myPieces = board.us(board.sideToMove());

    while (myPieces) {
        int sq = myPieces.pop();  // Get piece square index
        score += CENTER_CONTROL_BONUS[sq];   // Reward based on square
    }
    return score;
}

int key_center_squares_control(const chess::Position& board) {
    int score = 0;
    chess::Bitboard central_squares(0x1818000000);
    
    auto myPieces = board.us(board.sideToMove());

    while (myPieces) {
        int sq = myPieces.pop();
        if (central_squares.check(sq)) {
            score += EvalWeights::getWeight(EvalKey::KEY_CENTER); // Reward controlling these squares
        }
    }
    return score;
}
// precompute your file masks once at compile time:
static constexpr U64 FILE_MASKS[8] = {
    0x0101010101010101ULL << 0,  // file A
    0x0101010101010101ULL << 1,  // file B
    0x0101010101010101ULL << 2,  // file C
    0x0101010101010101ULL << 3,  // file D
    0x0101010101010101ULL << 4,  // file E
    0x0101010101010101ULL << 5,  // file F
    0x0101010101010101ULL << 6,  // file G
    0x0101010101010101ULL << 7   // file H
};

int rook_placement(const chess::Position& board) {
    const int side      = board.sideToMove();
    const U64 rooks     = board.pieces(chess::PieceType::ROOK, side).getBits();
    if (!rooks) return 0;

    const U64 occ       = board.occ().getBits();
    const U64 myPawns   = board.pieces(chess::PieceType::PAWN, side).getBits();

    const int wOpen     = EvalWeights::getWeight(EvalKey::OPEN_FILES);
    const int wSemi     = EvalWeights::getWeight(EvalKey::SEMI_OPEN_FILES);

    // build file‐masks in one 8‐step scan
    U64 openMask = 0, semiMask = 0;
    for (int f = 0; f < 8; ++f) {
        U64 m = FILE_MASKS[f];
        if ((m & occ) == 0) {
            openMask |= m;            // truly open file
        } else if ((m & myPawns) == 0) {
            semiMask |= m;            // no friendly pawn => semi-open
        }
    }
    return POPCOUNT64(rooks & openMask) * wOpen
         + POPCOUNT64(rooks & semiMask) * wSemi;
}

int development(const chess::Position& board) {
    // --- 1) Precompute bitboards once ---
    const U64 wKnights = board.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).getBits();
    const U64 wBishops = board.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).getBits();
    const U64 wRooks   = board.pieces(chess::PieceType::ROOK,   chess::Color::WHITE).getBits();
    const U64 wQueen   = board.pieces(chess::PieceType::QUEEN,  chess::Color::WHITE).getBits();

    const U64 bKnights = board.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK).getBits();
    const U64 bBishops = board.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).getBits();
    const U64 bRooks   = board.pieces(chess::PieceType::ROOK,   chess::Color::BLACK).getBits();
    const U64 bQueen   = board.pieces(chess::PieceType::QUEEN,  chess::Color::BLACK).getBits();

    const int   ph = phase(board);  // 1=open, 2=mid, 3=end

    // --- 2) All masks at compile time ---
    // White home squares
    constexpr U64 HOME_KNIGHT_W  = (1ULL<<1)  | (1ULL<<6);   // b1, g1
    constexpr U64 HOME_BISHOP_W  = (1ULL<<2)  | (1ULL<<5);   // c1, f1
    constexpr U64 HOME_ROOK_W    = (1ULL<<0)  | (1ULL<<7);   // a1, h1
    constexpr U64 HOME_QUEEN_W   =  1ULL<<3;                 // d1

    // Black home squares (mirrored on rank 8)
    constexpr U64 HOME_KNIGHT_B  = (1ULL<<57) | (1ULL<<62);  // b8, g8
    constexpr U64 HOME_BISHOP_B  = (1ULL<<58) | (1ULL<<61);  // c8, f8
    constexpr U64 HOME_ROOK_B    = (1ULL<<56) | (1ULL<<63);  // a8, h8
    constexpr U64 HOME_QUEEN_B   =  1ULL<<59;                // d8

    // --- 3) Fetch all weights once ---
    const int wKn  = EvalWeights::getWeight(EvalKey::UNDEV_KNIGHT);
    const int wBi  = EvalWeights::getWeight(EvalKey::UNDEV_BISHOP);
    const int wRk  = EvalWeights::getWeight(EvalKey::UNDEV_ROOK);
    const int wQ   = EvalWeights::getWeight(EvalKey::DEV_QUEEN);

    // --- 4) Count “undeveloped” pieces per color ---
    const int uw = POPCOUNT64(wKnights & HOME_KNIGHT_W);
    const int ub = POPCOUNT64(wBishops & HOME_BISHOP_W);
    const int ur = POPCOUNT64(wRooks   & HOME_ROOK_W);

    const int bw = POPCOUNT64(bKnights & HOME_KNIGHT_B);
    const int bb = POPCOUNT64(bBishops & HOME_BISHOP_B);
    const int br = POPCOUNT64(bRooks   & HOME_ROOK_B);

    // --- 5) Build penalty from knights, bishops, rooks ---
    int penalty = -wKn * ( uw - bw ) - wBi * ( ub - bb ) - wRk * ( ur - br );

    // --- 6) Early‐queen penalty in opening only ---
    if (ph == 1) {
        // if queen bitboard has any bit OUTSIDE its home square, it’s moved
        if ( (wQueen & ~HOME_QUEEN_W) != 0 ) penalty -= wQ;
        if ( (bQueen & ~HOME_QUEEN_B) != 0 ) penalty += wQ;
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
    chess::Bitboard myPieces = board.us(board.sideToMove());
    
    while (myPieces) {
        int sq = myPieces.pop();
        score += PIECE_ACTIVITY_BONUS[sq]; // Reward centralization
    }
    return score;
}
int evaluatePieceActivity(const chess::Position& board) {
    int score = 0;
    chess::Bitboard myPieces = board.us(board.sideToMove()),
                    enemyPieces = board.them(board.sideToMove());

    // Reward active pieces (knights, bishops, rooks, queens)
    while (myPieces) {
        chess::Square sq = myPieces.pop();
        auto attacks = chess::attacks::attackers(board, board.sideToMove(), sq);

        // Bonus for attacking enemy pieces
        score += (attacks & enemyPieces).count();
    }

    return score * EvalWeights::getWeight(EvalKey::ATTACK_ENEMY);
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
// **4. X-Ray Attack Detection**
inline bool isXRayAttack(U64 attacker, U64 blocker, U64 target) {
    return (attacker & blocker) && (attacker & target);
}
inline bool isTrappedPiece(const chess::Position& board, chess::Square pieceSq) {
    chess::Piece piece = board.at(pieceSq);
    if (piece == chess::Piece::NONE) return false;  // No piece → not trapped

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
    const auto occ = board.occ();
    chess::Piece p;

    switch (type) {
        case chess::KNIGHT: {
            auto knightAttacks = chess::attacks::knight(pieceSq);
            while (knightAttacks) {
                int sq = knightAttacks.pop();  // Extract LSB (fast)
                p = board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false; // The knight has a safe escape
                }
            }
            return true;
        }

        case chess::BISHOP: {
            auto bishopAttacks = chess::attacks::bishop(pieceSq, occ);
            while (bishopAttacks) {
                int sq = bishopAttacks.pop();  // Extract LSB (fast)
                p = board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false;
                }
            }
            return true;
        }

        case chess::ROOK: {
            auto rookAttacks = chess::attacks::rook(pieceSq, occ);
            while (rookAttacks) {
                int sq = rookAttacks.pop();  // Extract LSB (fast)
                p = board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false;
                }
            }
            return true;
        }

        case chess::QUEEN: {
            auto queenAttacks = chess::attacks::queen(pieceSq, occ);
            while (queenAttacks) {
                int sq = queenAttacks.pop();  // Extract LSB (fast)
                p = board.at(sq);
                if (p == chess::Piece::NONE || p.color() != side) {
                    return false;
                }
            }
            return true;
        }

        case chess::KING:
            // A king is trapped if it is completely surrounded **and attacked**
            return moves.empty() && !chess::attacks::attackers(board, ~side, pieceSq).empty();

        default:
            return false;  // Pawns and other cases do not need special checks
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
int evaluateTactics(const chess::Position& board) {
    chess::Square ksq = board.kingSq(board.sideToMove());
    int score = 0;

    auto myPieces = board.us(board.sideToMove());
    auto enemyPieces = board.them(board.sideToMove());

    // **1. Pin Check**
    auto pinnedPieces = myPieces;  // Cache the bitboard
    while (pinnedPieces) {
        int pieceSq = pinnedPieces.pop();
        if (isPinned(const_cast<chess::Position&>(board), chess::Square(pieceSq))) {
            score -= EvalWeights::getWeight(EvalKey::PINNED);
        }
    }

    // **2. Skewer Check** (Optimized)
    auto skewerAttackers = (board.pieces(chess::PieceType::ROOK, board.sideToMove()) |
                            board.pieces(chess::PieceType::QUEEN, board.sideToMove()));
    while (skewerAttackers) {
        int attackerSq = skewerAttackers.pop();

        auto targetPieces = enemyPieces;  // Cache enemy bitboard
        while (targetPieces) {
            int targetSq = targetPieces.pop();

            // Find pieces behind the target
            auto behindTarget = enemyPieces;
            while (behindTarget) {
                int behindTargetSq = behindTarget.pop();

                if (isSkewer(1ULL << attackerSq, 1ULL << targetSq, 1ULL << behindTargetSq)) {
                    score += EvalWeights::getWeight(EvalKey::SKEWERED);
                }
            }
        }
    }

    // **3. Discovered Check** (Direct Implementation)
    if (!board.move_stack.empty() && board.inCheck()) {
        const chess::Move& lastMove = board.move_stack.back();
        chess::Color currentStm = board.sideToMove();
        chess::Square kingSq = board.kingSq(currentStm);
        chess::Color attackerColor = ~currentStm;
        auto attackers = chess::attacks::attackers(board, attackerColor, kingSq);

        chess::Square movedTo = lastMove.to();
        bool discoveredCheck = false;

        while (attackers) {
            chess::Square attackerSq(attackers.pop());
            if (attackerSq != movedTo) {
                discoveredCheck = true;
                break;
            }
        }

        if (discoveredCheck) {
            score += EvalWeights::getWeight(EvalKey::DISCOVERED);
        }
    }

    // **4. X-Ray Attack Check**
    auto xRayPieces = (board.pieces(chess::PieceType::ROOK, board.sideToMove()) |
                       board.pieces(chess::PieceType::QUEEN, board.sideToMove()));
    while (xRayPieces) {
        int pieceSq = xRayPieces.pop();

        auto enemyTargets = enemyPieces;
        while (enemyTargets) {
            int enemyPieceSq = enemyTargets.pop();

            if (isXRayAttack(1ULL << pieceSq, 1ULL << enemyPieceSq, ksq.index())) {
                score += EvalWeights::getWeight(EvalKey::DISCOVERED);
            }
        }
    }

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

        score -= (whiteCount - 1) * EvalWeights::getWeight(EvalKey::DOUBLED);
        score += (blackCount - 1) * EvalWeights::getWeight(EvalKey::DOUBLED);
    }
    return score;
}

// **2. Backward Pawns**
// This function calculates a penalty or reward for backward pawns, which are pawns that are behind
// other pawns on the adjacent files and are unable to advance without being attacked.
int backward(U64 whitePawns, U64 blackPawns) {
    int score = 0;

    // Loop through each file (from A to H)
    for (int i = 0; i < 8; ++i) {
        // Create file mask for the current file
        U64 fileMask = FILES[i];
        // Create masks for the adjacent files (left and right)
        U64 leftFile = (i > 0) ? FILES[i - 1] : 0;
        U64 rightFile = (i < 7) ? FILES[i + 1] : 0;

        // Extract the pawns on the current file
        U64 whiteFile = whitePawns & fileMask;
        U64 blackFile = blackPawns & fileMask;

        // Extract pawns from the adjacent files
        U64 whiteAdjacent = whitePawns & (leftFile | rightFile);
        U64 blackAdjacent = blackPawns & (leftFile | rightFile);

        // Check for backward white pawns
        while (whiteFile) {
            int pos = lsb(whiteFile);  // Get the position of the least significant bit
            whiteFile &= whiteFile - 1;  // Clear the least significant bit

            // If there is no adjacent white pawn and the opponent has a pawn ahead, it's a backward pawn
            if (!(whiteAdjacent & (1ULL << pos)) && (blackPawns & (fileMask >> 8))) {
                score -= EvalWeights::getWeight(EvalKey::BACKWARD);
            }
        }

        // Check for backward black pawns
        while (blackFile) {
            int pos = lsb(blackFile);  // Get the position of the least significant bit
            blackFile &= blackFile - 1;  // Clear the least significant bit

            // If there is no adjacent black pawn and the opponent has a pawn behind, it's a backward pawn
            if (!(blackAdjacent & (1ULL << pos)) && (whitePawns & (fileMask << 8))) {
                score += EvalWeights::getWeight(EvalKey::BACKWARD);
            }
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
    // Get the pawns for the side to move
    chess::Bitboard pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove());

    // Isolated pawns on the east (no pawns on the east)
    chess::Bitboard isolatedEast = pawns & ~((pawns << 1) & ~chess::Bitboard(chess::File::FILE_H));

    // Isolated pawns on the west (no pawns on the west)
    chess::Bitboard isolatedWest = pawns & ~((pawns >> 1) & chess::Bitboard(chess::File::FILE_A));

    // Doubly isolated pawns are those isolated both on the east and west
    chess::Bitboard dblIsolated = isolatedEast & isolatedWest;

    // Count the number of doubly isolated pawns and apply evaluation weight
    return dblIsolated.count() * EvalWeights::getWeight(EvalKey::DBLISOLATED);
}
int weaks(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    U64 enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()).getBits();

    // Compute left (west) and right (east) pawns once
    U64 westPawns = west(pawns);
    U64 eastPawns = east(pawns);

    // Isolated pawns (no support from adjacent pawns)
    U64 isolated = pawns & ~(westPawns | eastPawns);

    // Pawns attacked by enemy pawns (north or south)
    U64 attacked = (north(pawns) & enemyPawns) | (south(pawns) & enemyPawns);

    // Weak pawns are either isolated or attacked by enemy pawns
    U64 weakPawns = isolated | attacked;

    return POPCOUNT64(weakPawns) * EvalWeights::getWeight(EvalKey::WEAK);
}

// **7. Pawn Race**
int pawnRace(const chess::Position& board) {
    auto pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove());
    int minDistance = 8;
    while (pawns) {
        chess::Square pos = pawns.pop();
        int rank = pos.rank();
        minDistance = std::min(minDistance, board.sideToMove() ? (7 - rank) : rank);
    }
    return (phase(board) == 3) ? (8 - minDistance) * EvalWeights::getWeight(EvalKey::PAWNRACE) : 8 - minDistance;
}

// **8. Color Weakness (Optimized)**
int weakness(const chess::Position& board) {
    U64 pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()).getBits();
    
    // Combine the pawns on dark and light squares, then count the pawns on weak squares
    U64 weakPawns = ~(pawns & (DARK_SQUARES | LIGHT_SQUARES));
    
    // Count the number of pawns on weak squares and multiply by the evaluation weight
    return POPCOUNT64(weakPawns) * EvalWeights::getWeight(EvalKey::WEAK);
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
    auto pawns = board.pieces(chess::PieceType::PAWN, board.sideToMove()), enemyPawns = board.pieces(chess::PieceType::PAWN, ~board.sideToMove()),
         blocked = ((pawns>>8)|(pawns<<8)) & enemyPawns;
    return blocked.count() * EvalWeights::getWeight(EvalKey::BLOCKED); // Higher penalty for blocked pawns
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
    if (board.move_stack.empty()) return 0;

    const chess::Move &lastMove = board.move_stack.back();
    if (lastMove.typeOf() != chess::Move::PROMOTION) return 0;

    board.pop();
    const chess::Piece movedPiece = board.at(lastMove.from());
    board.makeMove(lastMove);

    if (!(movedPiece == chess::Piece::WHITEPAWN || movedPiece == chess::Piece::BLACKPAWN)) return 0;

    const chess::Square to = lastMove.to();
    const bool isWhite = ~board.sideToMove();
    const U64 enemyKingBB = 1ULL << board.kingSq(~board.sideToMove()).index();
    const U64 toBB = 1ULL << to.index();
    const U64 enemyPieces = board.them(board.sideToMove()).getBits();
    const U64 friendlyPieces = board.us(board.sideToMove()).getBits();
    const int gamePhase = phase(board);

    const int reward = EvalWeights::getWeight(EvalKey::UNDERPROMOTE);

    if (getKingMobility(lsb(enemyKingBB), enemyPieces | friendlyPieces) == 0)
        return reward;

    if (chess::Square::back_rank(to, chess::Color(isWhite)) && (getKnightAttacks(to) & enemyKingBB))
        return reward;

    if (gamePhase == 1 && (toBB & enemyPieces))
        return reward;

    if (gamePhase == 3) {
        if (getKnightAttacks(to) & enemyPieces) return reward;
        if (isXRayAttack(toBB, enemyPieces, enemyKingBB)) return reward;
    }

    if (enemyPieces & getKnightAttacks(to))
        return reward;

    return (lastMove.promotionType().internal() != chess::PieceType::QUEEN) ? reward : 0;
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

int eval(chess::Position &board) {
    U64 hash = board.hash();
    // Faster lookup & insertion
    auto [it, inserted] = transposition.emplace(hash, 0);
    if (!inserted) return it->second;  // Already exists, return stored evaluation

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
    static const int lookup[] = {
        0, // NONE
        EvalWeights::getWeight(EvalKey::PAWN),
        EvalWeights::getWeight(EvalKey::KNIGHT),
        EvalWeights::getWeight(EvalKey::BISHOP),
        EvalWeights::getWeight(EvalKey::ROOK),
        EvalWeights::getWeight(EvalKey::QUEEN),
        0  // KING or others → 0
    };
    int idx = static_cast<int>(piece);
    return (idx >= 0 && idx < 7) ? lookup[idx] : 0;
}
