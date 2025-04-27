#include "search.h"
#include <vector>
#include <iostream>
#include <unordered_map>
#include <algorithm>

unsigned long long nodes = 0, tthits=0;

struct TranspositionEntry {
    int score;
    int depth;
    int flag;
    chess::Move bestMove;
    uint64_t key; // Store part of the hash for validation
};
constexpr size_t TT_SIZE = 1 << 20; // 1MB entries
namespace std {
    template <>
    struct hash<chess::Move> {
        uint16_t operator()(const chess::Move& m) const {
            return m.move();
        }
    };
}

constexpr int MAX_KILLERS_PER_PLY = 4;
std::vector<std::vector<chess::Move>> killerMoves;
std::unordered_map<chess::Move, int> moveHistory;
std::unordered_map<unsigned long long, TranspositionEntry> transpositionTable;

// Optional move score cache per ply
std::unordered_map<int, std::unordered_map<chess::Move, int>> moveScoreCache;
std::unordered_map<chess::Move, chess::Move> counterMove;
int moveScore(const chess::Position& RESTRICT board, const chess::Move& RESTRICT move, const int ply) {
    if (moveScoreCache.size()>=ply){
        auto v=moveScoreCache[ply];
        auto it=v.find(move);
        if (it!=v.end())return it->second;
    }

    // Check for killer moves directly using an array, avoiding std::find
    const auto& killers = killerMoves[ply];
    for (int i = 0; i < std::min(MAX_KILLERS_PER_PLY, static_cast<int>(killers.size())); ++i) {
        if (killers[i] == move) return 90000 - i * 1000;
    }

    if (board.isCapture(move)) return 80000 + piece_value(board.at(move.to()).type());
    {
        if (!board.move_stack.empty()){
            auto _board_move = board.move_stack.back();
            if (counterMove[_board_move] == move) return EvalWeights::getWeight(EvalKey::COUNTER);
        }
    }
    auto it_hist = moveHistory.find(move);
    return it_hist != moveHistory.end() ? it_hist->second : 0;
}

int calcExtensions(const chess::Position& board, const chess::Move& move) {
    // Consolidate branching into a single return statement for efficiency
    return (board.givesCheck(move) != chess::CheckType::NO_CHECK)
         + board.isCapture(move)
         + (move.typeOf() & chess::Move::ENPASSANT ? 1 : 0)
         + (move.promotionType() != chess::PieceType::underlying::NONE || move.from().rank() >= chess::Rank::RANK_6);
}

int search(chess::Position& RESTRICT board, const int depth, int alpha, const int beta, const int ply) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    if (moves.empty()) return board.inCheck() ? -MATE(ply) : 0;

    if (ply == 0) {
        killerMoves.resize(depth);
    } else if (ply >= static_cast<int>(killerMoves.size())) {
        killerMoves.resize(ply + 1);
    }

    unsigned long long boardHash = board.hash();
    auto ttIt = transpositionTable.find(boardHash);
    if (ttIt != transpositionTable.end()) {
        const auto& entry = ttIt->second;
        if (entry.depth >= depth) {
            if ((entry.flag == 0) || // exact score
                (entry.flag == 1 && entry.score <= alpha) || // lower bound
                (entry.flag == -1 && entry.score >= beta)) { // upper bound
                ++tthits;
                return entry.score;
            }
        }        
    }
    if (depth == 0) return eval(board);

    auto& cache = moveScoreCache[ply];
    // Push killer moves to the front
    std::sort(moves.begin(), moves.end(), [&board, &ply, &cache](const chess::Move& a, const chess::Move& b) {
        if (board.isCapture(a) && !board.isCapture(b)) return true;
        if (!board.isCapture(a) && board.isCapture(b)) return false;
        /*if (std::find(killerMoves[ply].begin(), killerMoves[ply].end(), a) != killerMoves[ply].end()) return true;
        if (std::find(killerMoves[ply].begin(), killerMoves[ply].end(), b) != killerMoves[ply].end()) return false;*/
        if (cache.find(a) == cache.end()) cache[a] = moveScore(board, a, ply);
        if (cache.find(b) == cache.end()) cache[b] = moveScore(board, b, ply);
        return cache[a] > cache[b];
    });


    bool isPVNode = true;
    int originalAlpha = alpha;
    int moveCount = 0;
    chess::Move bestMove;
    for (const auto& move : moves) {
        const bool doLMR = (moveCount > 0 && depth >= 3 && !board.isCapture(move));
        int ext = calcExtensions(board, move);
        int searchDepth = std::max(0, depth - 1-std::min(0, doLMR + ext));

        board.makeMove(move);
        ++nodes;

        int score = -search(board, searchDepth, -(isPVNode ? beta : alpha + 1), -alpha, ply + 1);
        if (!isPVNode && score > alpha && score < beta) {
            score = -search(board, searchDepth, -beta, -alpha, ply + 1);
        }
        board.unmakeMove(move);

        // Alpha-beta pruning and killer move updates
        if (score >= beta) {
            auto& killers = killerMoves[ply];
            if (std::find(killers.begin(), killers.end(), move) == killers.end()) {
                if (static_cast<int>(killers.size()) >= MAX_KILLERS_PER_PLY) killers.pop_back();
                killers.insert(killers.begin(), move);
            }
            transpositionTable[boardHash] = {score, depth, -1, move};
            if (!board.move_stack.empty()){
                auto _board_move = board.move_stack.back();
                counterMove[_board_move] = move;
            }
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            moveHistory[move] = std::min(moveHistory[move]+depth*depth, 1000000);
            bestMove = move;
        }

        ++moveCount;
        isPVNode = false;
    }
    transpositionTable[boardHash] = {alpha, depth, (alpha == originalAlpha ? 1 : (alpha < beta ? 0 : -1)), bestMove};
    return alpha;
}
void printPV(const chess::Position &board) {
    chess::Position b = board;
    chess::Move move;

    for (int i = 0; i < MAX_DEPTH; ++i) { // 64 is arbitrary depth cap
        auto it = transpositionTable.find(b.hash());
        if (it == transpositionTable.end()) break;

        const auto& entry = it->second;

        // If the entry has a best move, print it
        move = entry.bestMove;
        std::cout << move << " ";
        
        // Simulate making the move without actually modifying the position object
        b.makeMove(move);
    }
}
