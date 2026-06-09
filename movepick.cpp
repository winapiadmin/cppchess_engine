#include "movepick.h"
#include "eval.h"
#include "search.h"
#include <algorithm>
using namespace chess;
using engine::eval::piece_value;
namespace engine::movepick {

static Bitboard att(PieceType pt, Square sq, Bitboard occ) {
    switch (pt) {
    case BISHOP:
        return attacks::bishop(sq, occ);
    case ROOK:
        return attacks::rook(sq, occ);
    case QUEEN:
        return attacks::queen(sq, occ);
    default:
        return 0;
    }
}

Value see(Board &board, Move move) {
    if (move.type_of() == EN_PASSANT)
        return piece_value(PAWN);
    PieceType captured = board.at<PieceType>(move.to());
    if (captured == NO_PIECE_TYPE)
        return 0;
    PieceType attacker = board.at<PieceType>(move.from());
    Bitboard occ = board.occ() ^ (1ULL << move.from());
    Bitboard attackers = board.attackers(WHITE, move.to(), occ) | board.attackers(BLACK, move.to(), occ);
    Value gain[32];
    int d = 0;
    gain[d] = piece_value(captured);
    Color stm = ~board.side_to_move();
    while (++d < 32) {
        gain[d] = piece_value(attacker) - gain[d - 1];
        if (gain[d] < 0)
            break;
        attackers &= occ;
        Bitboard stmAttackers = attackers & board.occ(stm);
        if (!stmAttackers)
            break;
        Square sq = (Square)pop_lsb(stmAttackers);
        attacker = board.at<PieceType>(sq);
        if (attacker == NO_PIECE_TYPE)
            break;
        if (attacker == BISHOP || attacker == ROOK || attacker == QUEEN)
            attackers |= att(attacker, move.to(), occ);
        occ ^= (1ULL << sq);
        stm = ~stm;
    }
    while (--d)
        gain[d - 1] = -gain[d];
    return gain[0];
}

void orderMoves(Board &board, Movelist &moves, Move ttMove, int ply, const engine::search::Session &session, Move prevMove) {
    Value scores[300];
    size_t n = moves.size();
    for (size_t i = 0; i < n; ++i) {
        Move move = moves[i];
        if (move == ttMove)
            scores[i] = 10000;
        else if (board.isCapture(move)) {
            Value s = see(board, move);
            Value capturedVal = move.type_of() == EN_PASSANT ? piece_value(PAWN) : piece_value(board.at<PieceType>(move.to()));
            Value attackerVal = piece_value(board.at<PieceType>(move.from()));
            scores[i] = (s >= -50 ? 9000 : 4000) + std::max(s, Value(-50)) + (capturedVal * 10 - attackerVal) / 100;
        } else if (move == session.killerMoves[ply][0])
            scores[i] = 8500;
        else if (move == session.killerMoves[ply][1])
            scores[i] = 8000;
        else if (prevMove.is_ok() && move == session.counterMoves[prevMove.from_to()])
            scores[i] = 7500;
        else if (board.givesCheck(move) != CheckType::NO_CHECK)
            scores[i] = 7000;
        else
            scores[i] = session.historyHeuristic[move.from()][move.to()];
    }
    size_t limit = std::min(n, size_t(12));
    for (size_t i = 0; i + 1 < limit; ++i) {
        size_t best = i;
        for (size_t j = i + 1; j < n; ++j)
            if (scores[j] > scores[best])
                best = j;
        if (best != i) {
            std::swap(moves[i], moves[best]);
            std::swap(scores[i], scores[best]);
        }
    }
}
} // namespace engine::movepick