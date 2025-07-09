/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED
#include "chess.hpp"
#include "types.h"
#include "nnue/nnue_accumulator.h"
namespace Stockfish {
class Position {
   public:
    Position(const chess::Board& board) {
        b      = board;
        npm[1] = piece_value(chess::PieceType::KNIGHT)
                 * b.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK).count()
               + piece_value(chess::PieceType::BISHOP)
                   * b.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).count()
               + piece_value(chess::PieceType::ROOK)
                   * b.pieces(chess::PieceType::ROOK, chess::Color::BLACK).count()
               + piece_value(chess::PieceType::QUEEN)
                   * b.pieces(chess::PieceType::QUEEN, chess::Color::BLACK).count();
        npm[0] = piece_value(chess::PieceType::KNIGHT)
                 * b.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).count()
               + piece_value(chess::PieceType::BISHOP)
                   * b.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).count()
               + piece_value(chess::PieceType::ROOK)
                   * b.pieces(chess::PieceType::ROOK, chess::Color::WHITE).count()
               + piece_value(chess::PieceType::QUEEN)
                   * b.pieces(chess::PieceType::QUEEN, chess::Color::WHITE).count();
    }
    inline Stockfish::Bitboard pieces(Stockfish::Color c) const { return b.us(c).getBits(); }
    inline Stockfish::Bitboard pieces(Stockfish::Color c, Stockfish::PieceType pt) const {
        switch (pt)
        {
        case Stockfish::ALL_PIECES :
            return pieces(c);
        default :
            return b.pieces(chess::PieceType(static_cast<chess::PieceType::underlying>(pt - 1)), c)
              .getBits();
        }
    }
    inline Stockfish::Bitboard pieces(Stockfish::PieceType pt) const {
        switch (pt)
        {
        case Stockfish::ALL_PIECES :
            return b.occ().getBits();
        default :
            return b.pieces(chess::PieceType(static_cast<chess::PieceType::underlying>(pt - 1)))
              .getBits();
        }
    }
    inline Stockfish::Bitboard pieces() const { return b.occ().getBits(); }
    template<Stockfish::PieceType Pt>
    inline int count() const {
        switch (Pt)
        {
        case Stockfish::ALL_PIECES :
            return b.occ().count();
        default :
            return b.pieces(chess::PieceType(static_cast<chess::PieceType::underlying>(Pt - 1)))
              .count();
        }
    }
    template<Stockfish::PieceType Pt>
    inline int count(Stockfish::Color c) const {
        switch (Pt)
        {
        case Stockfish::ALL_PIECES :
            return b.occ().count();
        default :
            return b.pieces(chess::PieceType(static_cast<chess::PieceType::underlying>(Pt - 1)), c)
              .count();
        }
    }
    template<Stockfish::PieceType Pt>
    inline Stockfish::Square square(Stockfish::Color c) const {
        assert(count<Pt>(c) == 1);
        switch (Pt)
        {
        case Stockfish::ALL_PIECES :
            return static_cast<Stockfish::Square>(b.occ().lsb());
        default :
            return static_cast<Stockfish::Square>(
              b.pieces(chess::PieceType(static_cast<chess::PieceType::underlying>(Pt - 1)), c)
                .lsb());
        }
    }
    inline Stockfish::Color side_to_move() const {
        return (Stockfish::Color) static_cast<int>(b.sideToMove());
    }
    inline Stockfish::Piece piece_on(const Stockfish::Square sq) const {
        return to_engine_piece(b.at(sq).internal());
    }
    inline Stockfish::Value non_pawn_material() const { return npm[0] + npm[1]; }
    inline Stockfish::Value non_pawn_material(Stockfish::Color c) const { return npm[(int) c]; }
    inline int              rule50_count() const { return b.halfMoveClock(); }
    void                    do_move(chess::Move m) {
        Stockfish::DirtyPiece dp;
        dp.pc                     = to_engine_piece(b.at(m.from()).internal());
        dp.from                   = (Stockfish::Square) m.from().index();
        dp.to                     = (Stockfish::Square) m.to().index();
        dp.add_sq                 = Stockfish::SQ_NONE;
        Stockfish::Color us       = side_to_move();
        Stockfish::Color them     = ~us;
        Stockfish::Piece captured = m.typeOf() == m.ENPASSANT
                                                       ? Stockfish::make_piece(them, Stockfish::PAWN)
                                                       : piece_on(dp.to);
        if (m.typeOf() == m.CASTLING)
        {
            const bool king_side = m.to() > m.from();
            const auto rookTo = chess::Square::castling_rook_square(king_side, side_to_move());
            const auto kingTo = chess::Square::castling_king_square(king_side, side_to_move());
            dp.to        = (Stockfish::Square) kingTo.index();
            dp.remove_pc = dp.add_pc = make_piece(us, Stockfish::ROOK);
            dp.remove_sq             = (Stockfish::Square) m.to().index();
            dp.add_sq                = (Stockfish::Square) rookTo.index();
            captured                 = Stockfish::NO_PIECE;
        }
        else if (captured)
        {
            Stockfish::Square capsq =
              m.typeOf() == m.ENPASSANT
                                   ? Stockfish::Square(m.to().index() - (us == Stockfish::WHITE ? 8 : -8))
                                   : dp.to;
            dp.remove_pc = captured;
            dp.remove_sq = capsq;
        }
        else
            dp.remove_sq = Stockfish::SQ_NONE;
        if (m.typeOf() == m.PROMOTION)
        {
            dp.add_pc =
              make_piece(us,
                                            static_cast<Stockfish::PieceType>(
                           1 + m.promotionType()));  // promotionType() returns a PieceType,
            // which is 0 for NO_PIECE_TYPE, so we
            // add 1 to get the Piece
            // bonus: int + (OOP type) do the promotion of type integers because int can represent all the values in chess::PieceType (int8_t<int)
            // so int+(OOP type, castable to int) works while (OOP type, castable to int)+int doesn't work
            dp.add_sq = dp.to;
            dp.to = Stockfish::SQ_NONE;  // promotion moves are not allowed to have a to square
        }
        // Non pawn material
        if (m.typeOf() == m.PROMOTION)
        {
            npm[(int) us] += piece_value(m.promotionType());
        }
        // en passant and PxP are handled in piece_value
        if (b.isCapture(m))
            npm[(int) them] -= piece_value(b.at<chess::PieceType>(m.to()));

        b.makeMove(m);  // finally no
        stack.push(dp);
        //return dp;
    }
    void undo_move(chess::Move m) {
        b.unmakeMove(m);
        Stockfish::Color us   = side_to_move();
        Stockfish::Color them = ~side_to_move();
        if (m.typeOf() == m.PROMOTION)
        {
            npm[(int) us] -= piece_value(m.promotionType());
        }
        // en passant and PxP are handled in piece_value
        if (b.isCapture(m))
            npm[(int) them] += piece_value(b.at<chess::PieceType>(m.to()));
        stack.pop();
    }
    void                                    do_null_move() { b.makeNullMove(); }
    void                                    undo_null_move() { b.unmakeNullMove(); }
    chess::Board                            b;  // exposure to generate legal moves
    Stockfish::Eval::NNUE::AccumulatorStack stack;

   private:
    Stockfish::Value        npm[2] = {0, 0};  // non-pawn material for WHITE and BLACK
    inline Stockfish::Value piece_value(chess::PieceType pt) {  // fast and static check
        switch (pt)
        {
        case (int) chess::PieceType::KNIGHT :
            return Stockfish::KnightValue;
        case (int) chess::PieceType::BISHOP :
            return Stockfish::BishopValue;
        case (int) chess::PieceType::ROOK :
            return Stockfish::RookValue;
        case (int) chess::PieceType::QUEEN :
            return Stockfish::QueenValue;
        default :
            return 0;  // includes king
        }
    }
    inline Stockfish::Piece to_engine_piece(chess::Piece::underlying u) const {
        switch (u)
        {
        case chess::Piece::underlying::WHITEPAWN :
            return Stockfish::W_PAWN;
        case chess::Piece::underlying::WHITEKNIGHT :
            return Stockfish::W_KNIGHT;
        case chess::Piece::underlying::WHITEBISHOP :
            return Stockfish::W_BISHOP;
        case chess::Piece::underlying::WHITEROOK :
            return Stockfish::W_ROOK;
        case chess::Piece::underlying::WHITEQUEEN :
            return Stockfish::W_QUEEN;
        case chess::Piece::underlying::WHITEKING :
            return Stockfish::W_KING;
        case chess::Piece::underlying::BLACKPAWN :
            return Stockfish::B_PAWN;
        case chess::Piece::underlying::BLACKKNIGHT :
            return Stockfish::B_KNIGHT;
        case chess::Piece::underlying::BLACKBISHOP :
            return Stockfish::B_BISHOP;
        case chess::Piece::underlying::BLACKROOK :
            return Stockfish::B_ROOK;
        case chess::Piece::underlying::BLACKQUEEN :
            return Stockfish::B_QUEEN;
        case chess::Piece::underlying::BLACKKING :
            return Stockfish::B_KING;
        default :
            return Stockfish::NO_PIECE;
        }
    }
};
}
#endif
