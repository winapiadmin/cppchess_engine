#include "search.h"
unsigned long long nodes = 0;
std::unordered_map<U64, std::pair<int, int>> searchTT; // 🔄 Search TT (Eval, Depth)
std::unordered_map<int, chess::Move> killerMoves; // Killer Moves

inline int piece_value(chess::PieceType type) {
    switch (type.internal()) {
        case chess::PieceType::PAWN:   return 100;
        case chess::PieceType::KNIGHT: return 300;
        case chess::PieceType::BISHOP: return 320;
        case chess::PieceType::ROOK:   return 500;
        case chess::PieceType::QUEEN:  return 900;
        default: return 0;
    }
}

chess::Movelist order_moves(const chess::Board &board, int ply, bool capturesOnly = false) {
    chess::Movelist capture_moves, quiet_moves, moves;
    chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(capture_moves, board);
    chess::movegen::legalmoves<chess::movegen::MoveGenType::QUIET>(quiet_moves, board);

    std::vector<std::pair<chess::Move, int>> vec;

    // **Sort Captures Using MVV-LVA**
    for (int i = 0; i < capture_moves.size(); i++) {
        const auto move = capture_moves[i];
        const auto from = board.at(move.from()).type();
        const auto to = board.at(move.to()).type();
        int score = 10 * piece_value(to) - piece_value(from);
        if (move.promotionType() != chess::PieceType::NONE) score += 1000;
        if (board.isAttacked(move.to(), ~board.sideToMove())) score -= piece_value(from);
        vec.push_back({move, score});
    }
    std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    for (auto p : vec) moves.add(p.first);

    if (!capturesOnly) {
        vec.clear();
        for (int i = 0; i < quiet_moves.size(); i++) {
            const auto move = quiet_moves[i];
            int score = 0;
            if (killerMoves[ply] == move) score += 900;  // ✅ Use `ply`, not `board.ply()`
            if (move.promotionType() != chess::PieceType::NONE) score += 1000;
            if (board.isAttacked(move.to(), ~board.sideToMove())) score -= piece_value(board.at(move.from()).type());
            vec.push_back({move, score});
        }
        std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        for (auto p : vec) moves.add(p.first);
    }
    return moves;
}


// **2. Extensions (Only Extend When Necessary)**
int calculateExtensions(const chess::Board &board) {
    return board.inCheck() ? 1 : 0; // Only extend in check
}
int searchCaptures(chess::Board &board, int alpha, int beta, int ply) {
    if (board.isGameOver().first != chess::GameResultReason::NONE) return eval(board);

    int stand_pat = eval(board);
    if (stand_pat >= beta) return beta; // ✅ Stand Pat Cutoff (Stop Search)
    if (stand_pat > alpha) alpha = stand_pat; // ✅ Save Best Score

    chess::Movelist moves = order_moves(board, ply, true); // ✅ Capture-only ordering

    for (int i = 0; i < moves.size(); ++i) {
        board.makeMove(moves[i]);
        nodes++;

        int score = -searchCaptures(board, -beta, -alpha, ply + 1);
        board.pop();

        if (score >= beta) return beta; // ✅ Beta Cutoff
        alpha = std::max(alpha, score);
    }
    return alpha;
}
int search(chess::Board &board, int depth, int alpha, int beta, int ply) {
    if (depth == 0) return searchCaptures(board, alpha, beta, ply);

    U64 hash = board.hash();
    if (searchTT.find(hash) != searchTT.end() && searchTT[hash].second >= depth) {
        return searchTT[hash].first; // 🔄 TT Lookup with Depth Condition
    }

    if (board.isGameOver().first != chess::GameResultReason::NONE) return eval(board);

    if (ply == 0) { 
        searchTT.clear();  // ✅ Reset at Root
        killerMoves.clear();
    }

    // **✅ Null Move Pruning (Skip Useless Branches)**
    if (depth >= 3 && !board.inCheck() && eval(board) >= beta) return beta;

    // **✅ Move Ordering (TT Move First)**
    chess::Movelist moves = order_moves(board, ply);
    int bestScore = -MAX;
    chess::Move bestMove;

    for (int i = 0; i < moves.size(); ++i) {
        const auto move = moves[i];
        board.makeMove(move);
        nodes++;

        // **✅ Late Move Reductions (Reduce Search for Bad Moves)**
        int reduction = (i >= 4 && depth >= 3) ? 1 : 0;  
        int score = -search(board, depth - 1 - reduction + calculateExtensions(board), -beta, -alpha, ply + 1);

        board.pop();

        if (score >= beta) {
            killerMoves[ply] = move;
            searchTT[hash] = {beta, depth}; // 🔄 Store with Depth
            return beta;
        }

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
        alpha = std::max(alpha, score);
    }

    searchTT[hash] = {bestScore, depth}; // 🔄 Store with Depth
    return bestScore;
}

bool clearTransposition(){
    searchTT.clear();
    return searchTT.empty();
}
