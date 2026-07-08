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
inline Square least_valuable_attacker(const Position &board, Bitboard attackers, Color side) {
    Bitboard bb;

    bb = attackers & board.pieces(PAWN, side);
    if (bb)
        return Square(pop_lsb(bb));

    bb = attackers & board.pieces(KNIGHT, side);
    if (bb)
        return Square(pop_lsb(bb));

    bb = attackers & board.pieces(BISHOP, side);
    if (bb)
        return Square(pop_lsb(bb));

    bb = attackers & board.pieces(ROOK, side);
    if (bb)
        return Square(pop_lsb(bb));

    bb = attackers & board.pieces(QUEEN, side);
    if (bb)
        return Square(pop_lsb(bb));

    bb = attackers & board.pieces(KING, side);
    if (bb)
        return Square(pop_lsb(bb));

    return SQ_NONE;
}
Value see(Position &board, Move move) {
    Square from = move.from();
    Square to = move.to();

    PieceType captured = move.type_of() == EN_PASSANT ? PAWN : board.at<PieceType>(to);

    if (captured == NO_PIECE_TYPE)
        return 0;

    Bitboard occ = board.occ();
    occ ^= 1ULL << from;

    Bitboard attackers = board.attackers(WHITE, to, occ) | board.attackers(BLACK, to, occ);

    Value gain[32];
    PieceType attacker = board.at<PieceType>(move.from());

    gain[0] = piece_value(captured);

    Color stm = ~board.side_to_move();
    int d = 0;

    while (++d < 32) {

        // Charge the piece that just captured.
        gain[d] = piece_value(attacker) - gain[d - 1];

        if (gain[d] < 0)
            break;

        occ &= attackers;

        Bitboard stmAttackers = attackers & occ & board.occ(stm);
        if (!stmAttackers)
            break;

        Square sq = least_valuable_attacker(board, stmAttackers, stm);
        attacker = board.at<PieceType>(sq);

        occ ^= 1ULL << sq;

        // Recompute x-rays after EVERY removal.
        attackers = board.attackers(WHITE, to, occ) | board.attackers(BLACK, to, occ);

        stm = ~stm;
    }

    while (--d)
        gain[d - 1] = -std::max(-gain[d - 1], gain[d]);

    return gain[0];
}

void orderMoves(Position &board, Movelist &moves, Move ttMove, int ply, const engine::search::Session &session, Move prevMove) {
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
