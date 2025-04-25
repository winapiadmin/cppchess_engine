// Reverted to vector-based PV, optimized and simplified
#include "search.h"
#include <vector>
#include <iostream>
#include <unordered_map>
#include <algorithm>

unsigned long long nodes = 0;
std::vector<std::vector<chess::Move>> principalVariation;

struct TranspositionEntry {
    int score;
    int depth;
    int flag;
};

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

int moveScore(const chess::Position &board, const chess::Move& move, int ply, const chess::Move& pvMove) {
    if (move == pvMove) return 100000;
    auto& killers = killerMoves[ply];
    auto it = std::find(killers.begin(), killers.end(), move);
    if (it != killers.end()) return 90000 - static_cast<int>(it - killers.begin()) * 1000;
    if (board.isCapture(move)) return 80000 + piece_value(board.at(move.to()).type());
    auto it_hist = moveHistory.find(move);
    return it_hist != moveHistory.end() ? it_hist->second : 0;
}

int calcExtensions(const chess::Position& board, const chess::Move& move){
    int ext = 0;
    if (board.givesCheck(move) != chess::CheckType::NO_CHECK) ++ext;
    if (board.isCapture(move)) ++ext;
    if (move.promotionType() != chess::PieceType::underlying::NONE) ++ext;
    if (move.typeOf() & chess::Move::ENPASSANT) ++ext;
    if (move.from().rank() >= chess::Rank::RANK_6 && move.promotionType() != chess::PieceType::underlying::NONE) ++ext;
    return ext;
}

int search(chess::Position &board, int depth, int alpha, int beta, int ply) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    if (moves.empty()) return board.inCheck() ? -MATE(ply) : 0;

    if (ply == 0) {
        principalVariation.clear();
        principalVariation.reserve(depth);
        principalVariation.resize(depth);
        /*transpositionTable.clear();
        transpositionTable.reserve(1 << 20);*/
        killerMoves.clear();
        killerMoves.resize(depth);
        /*moveHistory.clear();
        moveHistory.reserve(1 << 14);*/
        moveScoreCache.clear();
    }
    if (ply >= static_cast<int>(principalVariation.size())) {
        principalVariation.resize(ply + 1);
        killerMoves.resize(ply + 1);
        killerMoves[ply].clear();
    }

    unsigned long long boardHash = board.hash();
    auto ttIt = transpositionTable.find(boardHash);
    if (ttIt != transpositionTable.end()) {
        const auto& entry = ttIt->second;
        if (entry.depth >= depth) {
            if (entry.flag == 0) return entry.score;
            if (entry.flag == 1 && entry.score <= alpha) return entry.score;
            if (entry.flag == -1 && entry.score >= beta) return entry.score;
        }
    }
    if (depth == 0) return eval(board);

    const chess::Move pvMove = (!principalVariation[ply].empty()) ? principalVariation[ply][0] : chess::Move();
    auto& cache = moveScoreCache[ply];

    std::sort(moves.begin(), moves.end(), [&board, &pvMove, &ply, &cache](const chess::Move& a, const chess::Move& b) {
        if (cache.find(a) == cache.end()) cache[a] = moveScore(board, a, ply, pvMove);
        if (cache.find(b) == cache.end()) cache[b] = moveScore(board, b, ply, pvMove);
        return cache[a] > cache[b];
    });

    bool isPVNode = true;
    int originalAlpha = alpha;
    int moveCount = 0;
    std::vector<chess::Move> bestLine;

    for (const auto& move : moves) {
        const bool doLMR = (moveCount > 0 && depth >= 3 && !board.isCapture(move));
        int ext = calcExtensions(board, move);
        int searchDepth = std::max(0, std::min(depth - 1, depth - 1 - doLMR + (ext > 0)));

        board.makeMove(move);
        ++nodes;

        int score = -search(board, searchDepth, -(isPVNode ? beta : alpha + 1), -alpha, ply + 1);
        if (!isPVNode && score > alpha && score < beta) {
            score = -search(board, searchDepth, -beta, -alpha, ply + 1);
        }

        const auto& childPV = (ply + 1 < principalVariation.size()) ? principalVariation[ply + 1] : std::vector<chess::Move>();
        board.unmakeMove(move);

        if (score >= beta) {
            auto& killers = killerMoves[ply];
            if (std::find(killers.begin(), killers.end(), move) == killers.end()) {
                if (static_cast<int>(killers.size()) >= MAX_KILLERS_PER_PLY) killers.pop_back();
                killers.insert(killers.begin(), move);
            }
            transpositionTable[boardHash] = {score, depth, -1};
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            moveHistory[move] += depth * depth;
            bestLine.clear();
            bestLine.push_back(move);
            bestLine.insert(bestLine.end(), childPV.begin(), childPV.end());
        }

        ++moveCount;
        isPVNode = false;
    }

    if (ply < static_cast<int>(principalVariation.size())) {
        principalVariation[ply] = std::move(bestLine);
    }
    transpositionTable[boardHash] = {alpha, depth, (alpha == originalAlpha ? 1 : (alpha < beta ? 0 : -1))};
    return alpha;
}

void printPV(const chess::Position& board) {
    if (!principalVariation.empty() && !principalVariation[0].empty()) {
        chess::Position pos = board;
        for (const auto& move : principalVariation[0]) {
            std::cout << move << " ";
            pos.makeMove(move);
        }
        std::cout << '\n';
    }
}
