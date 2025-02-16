#include "eval.h"

std::unordered_map<U64, int> transposition;  // Faster lookup

// Declare only missing functions
int evaluatePawnStructure(const chess::Board& board);
int evaluatePieces(const chess::Board& board);

int eval(chess::Board &board) {
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
    int eval = (Pawn * (pieceCount[0] - pieceCount[5])) +
               (Knight * (pieceCount[1] - pieceCount[6])) +
               (Bishop * (pieceCount[2] - pieceCount[7])) +
               (Rook * (pieceCount[3] - pieceCount[8])) +
               (Queen * (pieceCount[4] - pieceCount[9]));
    
    // Evaluate positional aspects
    eval += evaluatePawnStructure(board);
    eval += evaluatePieces(board);
    eval += evaluateKingSafety(board);
    eval += evaluateTactics(board);
    board.makeNullMove();
    // Evaluate positional aspects
    eval -= evaluatePawnStructure(board);
    eval -= evaluatePieces(board);
    eval -= evaluateKingSafety(board);
    eval -= evaluateTactics(board);
    board.unmakeNullMove();
    it->second = eval;  // Store result
    return eval;
}

// Use `const chess::Board&` to avoid copies
int evaluatePawnStructure(const chess::Board& board) {
    chess::Board b=board;
    return -(isolated(board) + dblisolated(board) + weaks(board) + blockage(board) +
             holes(board) + underpromote(b) + weakness(board) + evaluatePawnRams(board)) +
           (pawnIslands(board) + pawnRace(board) + pawnShield(board) + pawnStorm(board) +
            pawnLevers(board) + outpost(board) + evaluateUnfreePawns(board) + evaluateOpenPawns(board));
}

// Use `const chess::Board&` to avoid copies
int evaluatePieces(const chess::Board& board) {
    return -(evaluateBadBishops(board) + evaluateTrappedPieces(board) + evaluateKnightForks(board)) +
           (evaluateFianchetto(board) + evaluateRooksOnFiles(board));
}

