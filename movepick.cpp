#include "movepick.hpp"

namespace movepick
{
    // 1. SEE

    // Return type changed to chess::Square for consistency
    chess::Square select_least_valuable(chess::Board &board, chess::Bitboard bb)
    {
        if (bb.empty())
            return chess::Square::NO_SQ; // Use NO_SQ sentinel for no square

        int least_value = 5000;
        chess::Square least_valuable_square = chess::Square::NO_SQ;

        while (bb)
        {
            int idx = bb.pop(); // idx is int index of bitboard square
            chess::Square sq(idx);
            int value = piece_value(board.at<chess::PieceType>(sq));
            if (value < least_value)
            {
                least_value = value;
                least_valuable_square = sq;
            }
        }
        return least_valuable_square;
    }

    int SEE(chess::Board &board, chess::Move move)
    {
        constexpr int max_gain_size = MAX_PLY + 1; // To avoid overflow in gain[d + 1]
        std::array<int, max_gain_size> gain{};
        int d = 0;
        chess::Color side = board.sideToMove();
        chess::Bitboard occ = board.occ();

        chess::Square target_sq = move.to();

        // Initial gain setup:
        if (move.typeOf() & chess::Move::PROMOTION)
        {
            gain[0] = piece_value(move.promotionType());
            if (board.isCapture(move))
                gain[0] += piece_value(board.at<chess::PieceType>(target_sq));
        }
        else if (move.typeOf() == chess::Move::ENPASSANT)
        {
            // We'll treat it normally below, so just assign captured pawn value now:
            gain[0] = piece_value(chess::PieceType::PAWN);
        }
        else
        {
            gain[0] = piece_value(board.at<chess::PieceType>(target_sq));
        }

        chess::Square from_sq = move.from();

        // Remove the moving piece from occupancy:
        occ.clear(from_sq.index());

        // Special case for en passant: remove the captured pawn square here:
        if (move.typeOf() == chess::Move::ENPASSANT)
        {
            chess::Square ep_captured_sq(target_sq.file(), from_sq.rank()); // square behind target
            occ.clear(ep_captured_sq.index());
        }

        chess::Square next_attacker_sq = from_sq;

        while (++d < MAX_PLY)
        {
            side = ~side;

            // Find all attackers of target square from side to move:
            chess::Bitboard attackers = chess::attacks::attackers(board, side, target_sq) & occ;

            // Include x-ray attacks for sliding pieces:
            chess::Bitboard rook_attacks = chess::attacks::rook(target_sq, occ);
            chess::Bitboard bishop_attacks = chess::attacks::bishop(target_sq, occ);

            attackers |= rook_attacks & (board.pieces(chess::PieceType::ROOK, side) | board.pieces(chess::PieceType::QUEEN, side));
            attackers |= bishop_attacks & (board.pieces(chess::PieceType::BISHOP, side) | board.pieces(chess::PieceType::QUEEN, side));

            if (attackers.empty())
                break;

            next_attacker_sq = select_least_valuable(board, attackers);
            if (next_attacker_sq == chess::Square::NO_SQ)
                break;

            int captured_value = piece_value(board.at<chess::PieceType>(next_attacker_sq));
            gain[d] = -gain[d - 1] + captured_value;

            if (std::max(-gain[d - 1], gain[d]) < 0)
                break;

            occ.clear(next_attacker_sq.index());
        }

        while (--d > 0)
        {
            gain[d] = std::max(-gain[d + 1], gain[d]);
        }

        return gain[0];
    }

    // 2. Killer moves
    chess::Move killerMoves[MAX_PLY][2] = {};

    void updateKillerMoves(chess::Move m, int ply)
    {
        if (killerMoves[ply][0] != m)
        {
            killerMoves[ply][1] = killerMoves[ply][0];
            killerMoves[ply][0] = m;
        }
    }

    // 3. History heuristic
    int historyHeuristic[64][64] = {}; // from-square to to-square

    void updateHistoryHeuristic(chess::Move m, int depth)
    {
        historyHeuristic[m.from().index()][m.to().index()] += depth * depth;
    }

    void orderMoves(chess::Board &board, chess::Movelist &moves, chess::Move ttMove, chess::Move pvMove, int ply)
    {
        for (auto &move : moves)
        {
            if (move == ttMove)
                move.setScore(10000);
            else if (move == pvMove)
                move.setScore(9000);
            else if (board.isCapture(move))
                move.setScore(SEE(board, move));
            else if (move == killerMoves[ply][0])
                move.setScore(8500);
            else if (move == killerMoves[ply][1])
                move.setScore(8000);
            else
                move.setScore(historyHeuristic[move.from().index()][move.to().index()]);
        }
        // Sort moves by score in descending order
        std::stable_sort(moves.begin(), moves.end(),
                         [](const chess::Move &a, const chess::Move &b)
                         { return a.score() > b.score(); });
    }

    void qOrderMoves(chess::Board &board, chess::Movelist &moves)
    {
        // clang-format off
        int16_t promotion_table[4][8] = {
            {-58, -38, -13, -28, -31, -27, -63, -99}, // N
            {-14, -21, -11,  -8,  -7,  -9, -17, -24}, // B
            { 13,  10,  18,  15,  12,  12,   8,   5}, // R
            { -9,  22,  22,  27,  27,  19,  10,  20}, // Q
        };
        // clang-format on

        for (auto &move : moves)
        {
            if (board.isCapture(move)) // Simple MVV-LVA
            {
                move.setScore(SEE(board, move));
            }
            else if (move.typeOf() & chess::Move::PROMOTION)
            {
                int promoValue = piece_value(move.promotionType());
                int captureValue = board.isCapture(move) ? piece_value(board.at<chess::PieceType>(move.to())) : 0;
                move.setScore(promoValue + captureValue + promotion_table[move.promotionType() - 1][move.to().file()]);
            }
            else
            {
                move.setScore(historyHeuristic[move.from().index()][move.to().index()]);
            }
        }
        // Sort moves by score in descending order
        std::stable_sort(moves.begin(), moves.end(),
                         [](const chess::Move &a, const chess::Move &b)
                         { return a.score() > b.score(); });
    }
}
