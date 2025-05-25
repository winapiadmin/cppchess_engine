/**
 * @brief Reorders and scores moves for improved search efficiency in chess move ordering
 *
 * This function assigns scores to moves based on various heuristics to prioritize
 * potentially more promising moves during chess move search. Scoring considers:
 * - Captures (using Most Valuable Victim - Least Valuable Attacker)
 * - Counter moves
 * - Killer moves
 * - Historical move performance
 *
 * @param board The current chess board position
 * @param moves A list of moves to be scored and reordered
 * @param ply The current search depth/ply
 */
#include "move_ordering.hpp"
namespace move_ordering
{
chess::Square getLeastValuable(chess::Bitboard attackers, const chess::Position &board, chess::Color side)
{
  static const std::array<int, 6> pieceOrder = {
    (int)chess::PieceType::PAWN,
    (int)chess::PieceType::KNIGHT,
    (int)chess::PieceType::BISHOP,
    (int)chess::PieceType::ROOK,
    (int)chess::PieceType::QUEEN,
    (int)chess::PieceType::KING
  };

  for (int pt : pieceOrder)
  {
    while(attackers)
    {
	chess::Square sq=attackers.pop();
      if (board.at(sq).type() == (chess::PieceType::underlying)pt &&
          board.at(sq).color() == side)
        return sq;
    }
  }

  // fallback — if no attacker found (shouldn't happen)
  return chess::Square::NO_SQ;
}
chess::Bitboard recomputeAttackers(chess::Square target, chess::Bitboard occupied, const chess::Board& board)
{
	chess::Bitboard attackers = 0;

  while (occupied)
  {
    auto sq=occupied.pop();
    const chess::Piece piece = board.at(sq);
    if (piece == chess::Piece::NONE) continue;

    chess::Color c = piece.color();
    chess::PieceType pt = piece.type();

    switch (pt)
    {
	    case (int)chess::PieceType::PAWN:
        if (chess::attacks::pawn(c, sq).check(target.index()))
          attackers |= chess::Bitboard::fromSquare(sq);
        break;

	    case (int)chess::PieceType::KNIGHT:
        if (chess::attacks::knight(sq).check(target.index()))
          attackers |= chess::Bitboard::fromSquare(sq);
        break;

	    case (int)chess::PieceType::BISHOP:
        if (chess::attacks::bishop(sq, occupied).check(target.index()))
          attackers |= chess::Bitboard::fromSquare(sq);
        break;

	    case (int)chess::PieceType::ROOK:
        if (chess::attacks::rook(sq, occupied).check(target.index()))
          attackers |= chess::Bitboard::fromSquare(sq);
        break;

	    case (int)chess::PieceType::QUEEN:
        if (chess::attacks::queen(sq, occupied).check(target.index()))
          attackers |= chess::Bitboard::fromSquare(sq);
        break;

	    case (int)chess::PieceType::KING:
        if (chess::attacks::king(sq).check(target.index()))
          attackers |= chess::Bitboard::fromSquare(sq);
        break;

      default:
        break;
    }
  }

  return attackers;
}
int see(const chess::Position &board, chess::Move move)
{
  int to = move.to().index();
  int gain[32], d = 0;
  chess::Bitboard occupied = board.occ();
  chess::Color stm = board.sideToMove();
  gain[0] = piece_value(board.at(to).type());
  chess::Bitboard attackers = chess::attacks::attackers(board, ~stm, move.to());
  
  while (attackers)
  {
    chess::Square from = getLeastValuable(attackers, board, stm);
    if (from == chess::Square::NO_SQ) break; // safety check

    gain[++d] = piece_value(board.at(from).type()) - gain[d - 1];

    if (std::max(-gain[d - 1], gain[d]) < 0)
      break;

    occupied ^= 1ULL << from.index();
    attackers = recomputeAttackers(to, occupied, board);
    stm = ~stm;
  }

  for (int i = d - 1; i >= 0; --i)
  {
    gain[i] = -std::max(-gain[i], gain[i + 1]);
  }

  return gain[0];
}

  void moveOrder(chess::Position &board, chess::Movelist &moves, int ply)
  {
    const chess::Move prev = board.move_stack.empty() ? chess::Move() : board.move_stack.back();
    const int side = (int)board.sideToMove();

    // Pre-calculate common values used in the loop
    constexpr int killer1_score = SCORE_KILLER;
    constexpr int killer2_score = SCORE_KILLER / 2;
    const chess::Move &killer1 = killerMoves[0][ply];
    const chess::Move &killer2 = killerMoves[1][ply];
    const chess::Move &counter = counterMove[prev.from().index()][prev.to().index()];

    for (int i = 0; i < moves.size(); i++)
    {
      chess::Move &move = moves[i];
      int score = 0;

      // Capture scoring
      if (board.isCapture(move))
      {
        //auto victim = board.at(move.to()).type();
        //auto attacker = board.at(move.from()).type();
        score += SCORE_CAPTURE + see(board, move);
      }

      // Counter move bonus (simple equality check)
      if (move == counter)
      {
        score += SCORE_COUNTER;
      }

      // Killer move bonus (simple equality checks)
      if (move == killer1)
        score += killer1_score;
      else if (move == killer2)
        score += killer2_score;

      // History heuristic
      score += history[side][move.from().index()][move.to().index()] * SCORE_HISTORY;

      // Promotion bonus
      if (move.typeOf() == chess::Move::PROMOTION)
        score += piece_value(move.promotionType());
      
      // Set the score
      move.setScore(score);
    }

    // Sort moves in descending order of score
    std::sort(moves.begin(), moves.end(), [](const chess::Move &a, const chess::Move &b)
              {
                return a.score() > b.score();
              });
  }
}
