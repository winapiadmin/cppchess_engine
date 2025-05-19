#include "eval.h"
// Utilities

// Function to flip a FEN string
std::string flipFEN(const std::string &fen)
{
    // Split the FEN string into its components
    size_t first_space = fen.find(' ');
    std::string piece_placement = fen.substr(0, first_space);
    std::string remaining = fen.substr(first_space + 1);

    // Reverse the piece placement
    std::string flipped;
    for (auto it = piece_placement.rbegin(); it != piece_placement.rend(); ++it)
    {
        if (std::isdigit(*it))
        {
            flipped += *it;
        }
        else if (*it == '/')
        {
            flipped += '/';
        }
        else
        {
            flipped += std::islower(*it) ? std::toupper(*it) : std::tolower(*it);
        }
    }
    std::reverse(flipped.begin(), flipped.end());

    // Toggle the side to move
    size_t second_space = remaining.find(' ');
    char side_to_move = remaining[0] == 'w' ? 'b' : 'w';
    std::string rest = remaining.substr(second_space);

    // Adjust castling rights
    size_t third_space = rest.find(' ');
    std::string castling = rest.substr(1, third_space - 1);
    for (char &c : castling)
    {
        if (c == 'K')
            c = 'k';
        else if (c == 'Q')
            c = 'q';
        else if (c == 'k')
            c = 'K';
        else if (c == 'q')
            c = 'Q';
    }
    if (castling.empty())
        castling = "-";

    // Adjust en passant square
    size_t fourth_space = rest.find(' ', third_space + 1);
    std::string en_passant = rest.substr(third_space + 1, fourth_space - third_space - 1);
    if (en_passant != "-")
    {
        en_passant[1] = en_passant[1] == '3' ? '6' : en_passant[1] == '6' ? '3'
                                                                          : en_passant[1];
    }

    // Combine all parts to form the flipped FEN
    return flipped + " " + side_to_move + " " + castling + " " + en_passant + rest.substr(fourth_space);
}
chess::Position colorflip(const chess::Position &Position)
{
    return chess::Position(flipFEN(Position.getFen()));
}
typedef int (*func_1)(const chess::Position &, const chess::Square &);
typedef int (*func_2)(const chess::Position &, const chess::Square &, int);
int sum(const chess::Position &pos, func_1 func)
{
    int total = 0;
    for (int x = 0; x < 64; ++x)
            total += func(pos, x);
    return total;
}
int sum(const chess::Position &pos, func_2 func, int param)
{
    int total = 0;
    for (int x = 0; x < 64; ++x)
            total += func(pos, x, param);
    return total;
}

// Main evaluation
int piece_value_bonus(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ, int mg = 0)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, piece_value_bonus, 0);
    static constexpr std::array<int, 5> mg_values = {124, 781, 825, 1276, 2538};
    static constexpr std::array<int, 5> eg_values = {206, 854, 915, 1380, 2682};
    chess::PieceType i = pos.at(square).type();
    return (i != chess::PieceType::NONE) ? (mg ? mg_values[i] : eg_values[i]) : 0;
}
inline int piece_value_mg(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, piece_value_mg);
    return piece_value_bonus(pos, square, true);
}

int psqt_bonus(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ, int mg = 0)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, psqt_bonus, mg);

    static const std::vector<std::vector<std::vector<int>>> bonus[2] = {
        {// Endgame (mg == 0)
         {{-96, -65, -49, -21}, {-67, -54, -18, 8}, {-40, -27, -8, 29}, {-35, -2, 13, 28}, {-45, -16, 9, 39}, {-51, -44, -16, 17}, {-69, -50, -51, 12}, {-100, -88, -56, -17}},
         {{-57, -30, -37, -12}, {-37, -13, -17, 1}, {-16, -1, -2, 10}, {-20, -6, 0, 17}, {-17, -1, -14, 15}, {-30, 6, 4, 6}, {-31, -20, -1, 1}, {-46, -42, -37, -24}},
         {{-9, -13, -10, -9}, {-12, -9, -1, -2}, {6, -8, -2, -6}, {-6, 1, -9, 7}, {-5, 8, 7, -6}, {6, 1, -7, 10}, {4, 5, 20, -5}, {18, 0, 19, 13}},
         {{-69, -57, -47, -26}, {-55, -31, -22, -4}, {-39, -18, -9, 3}, {-23, -3, 13, 24}, {-29, -6, 9, 21}, {-38, -18, -12, 1}, {-50, -27, -24, -8}, {-75, -52, -43, -36}},
         {{1, 45, 85, 76}, {53, 100, 133, 135}, {88, 130, 169, 175}, {103, 156, 172, 172}, {96, 166, 199, 199}, {92, 172, 184, 191}, {47, 121, 116, 131}, {11, 59, 73, 78}}},
        {// Middlegame (mg == 1)
         {{-175, -92, -74, -73}, {-77, -41, -27, -15}, {-61, -17, 6, 12}, {-35, 8, 40, 49}, {-34, 13, 44, 51}, {-9, 22, 58, 53}, {-67, -27, 4, 37}, {-201, -83, -56, -26}},
         {{-53, -5, -8, -23}, {-15, 8, 19, 4}, {-7, 21, -5, 17}, {-5, 11, 25, 39}, {-12, 29, 22, 31}, {-16, 6, 1, 11}, {-17, -14, 5, 0}, {-48, 1, -14, -23}},
         {{-31, -20, -14, -5}, {-21, -13, -8, 6}, {-25, -11, -1, 3}, {-13, -5, -4, -6}, {-27, -15, -4, 3}, {-22, -2, 6, 12}, {-2, 12, 16, 18}, {-17, -19, -1, 9}},
         {{3, -5, -5, 4}, {-3, 5, 8, 12}, {-3, 6, 13, 7}, {4, 5, 9, 8}, {0, 14, 12, 5}, {-4, 10, 6, 8}, {-5, 6, 10, 8}, {-2, -2, 1, -2}},
         {{271, 327, 271, 198}, {278, 303, 234, 179}, {195, 258, 169, 120}, {164, 190, 138, 98}, {154, 179, 105, 70}, {123, 145, 81, 31}, {88, 120, 65, 33}, {59, 89, 45, -1}}}};

    static const std::vector<std::vector<int>> pbonus[2] = {
        {// Endgame (mg == 0)
         {0, 0, 0, 0, 0, 0, 0, 0},
         {-10, -6, 10, 0, 14, 7, -5, -19},
         {-10, -10, -10, 4, 4, 3, -6, -4},
         {6, -2, -8, -4, -13, -12, -10, -9},
         {10, 5, 4, -5, -5, -5, 14, 9},
         {28, 20, 21, 28, 30, 7, 6, 13},
         {0, -11, 12, 21, 25, 19, 4, 7},
         {0, 0, 0, 0, 0, 0, 0, 0}},
        {// Middlegame (mg == 1)
         {0, 0, 0, 0, 0, 0, 0, 0},
         {3, 3, 10, 19, 16, 19, 7, -5},
         {-9, -15, 11, 15, 32, 22, 5, -22},
         {-4, -23, 6, 20, 40, 17, 4, -8},
         {13, 0, -13, 1, 11, -2, -13, 5},
         {5, -12, -7, 22, -8, -5, -15, -8},
         {-7, 7, -3, -13, 5, -16, 10, -8},
         {0, 0, 0, 0, 0, 0, 0, 0}}};

    chess::PieceType p = pos.at(square).type();
    if (p == chess::PieceType::NONE)
        return 0;
    int index = p;
    return (index == 0) ? pbonus[mg][7 - square.rank()][square.file()] : bonus[mg][index - 1][7 - square.rank()][std::min(square.file(), chess::File(7 - square.file()))];
}
inline int psqt_mg(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, psqt_mg);
    return psqt_bonus(pos, square, true);
}
int imbalance(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, imbalance);

    static const std::vector<std::vector<int>> qo = {
        {0}, {40, 38}, {32, 255, -62}, {0, 104, 4, 0}, {-26, -2, 47, 105, -208}, {-189, 24, 117, 133, -134, -6}};
    static const std::vector<std::vector<int>> qt = {
        {0}, {36, 0}, {9, 63, 0}, {59, 65, 42, 0}, {46, 39, 24, -24, 0}, {97, 100, -42, 137, 268, 0}};

    const std::string piece_map = ".PNBRQ.pnbrq";
    int j = piece_map.find(pos.at(square));

    if (j < 0 || j > 5)
        return 0; // Ignore out-of-bounds and kings

    int bishop[2] = {0, 0}, v = 0;

    for (chess::Square s; s.index() < 64; ++s)
    {
        int i = piece_map.find(pos.at(s));

        if (i == std::string::npos)
            continue; // Ignore empty squares

        if (i == 9)
            bishop[0]++;
        if (i == 3)
            bishop[1]++;
        if (i % 6 > j)
            continue;

        v += (i > 5) ? qt[j][i - 6] : qo[j][i];
    }

    if (bishop[0] > 1)
        v += qt[j][0];
    if (bishop[1] > 1)
        v += qo[j][0];

    return v;
}
inline int bishop_pair(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (pos.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).count() < 2)
        return 0;
    if (square == chess::Square::underlying::NO_SQ)
        return 1438;
    return pos.at(square) == chess::Piece::WHITEBISHOP;
}
inline int imbalance_total(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    int v = 0;
    v += imbalance(pos) - imbalance(colorflip(pos));
    v += bishop_pair(pos) - bishop_pair(colorflip(pos));
    return v >> 4;
}
int isolated(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, isolated);
    if (pos.at(square) != chess::Piece::WHITEPAWN)
        return 0;
    int file = square.file();
    chess::Bitboard left(chess::File(file+1)),
	    	    right(chess::File(file-1)),
		    pawns=pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE),
		    adj=left|right;
    if (pawns&adj)return 0;
    return 1;
}
int doubled_isolated(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, doubled_isolated);

    if (pos.at(square) != chess::Piece::WHITEPAWN)
        return 0;

    if (isolated(pos, square))
    {
        int obe = 0, eop = 0, ene = 0;
        int file = square.file();
        int rank = square.rank();

        for (int y = 0; y < 8; y++)
        {
            if (y > rank && pos.at(chess::Square(chess::File(file), y)) == chess::Piece::WHITEPAWN)
                obe++;
            if (y < rank && pos.at(chess::Square(chess::File(file), y)) == chess::Piece::BLACKPAWN)
                eop++;
            if ((file > 0 && pos.at(chess::Square(chess::File(file - 1), y)) == chess::Piece::BLACKPAWN) ||
                (file < 7 && pos.at(chess::Square(chess::File(file + 1), y)) == chess::Piece::BLACKPAWN))
                ene++;
        }

        if (obe > 0 && ene == 0 && eop > 0)
            return 1;
    }

    return 0;
}
int backward(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, backward);
    if (pos.at(square) != chess::Piece::WHITEPAWN)
        return 0;
    int file = square.file();
    int rank = square.rank();

    for (int y = rank; y < 8; y++)
    {
        if ((file && pos.at(chess::Square(chess::File(file - 1), y)) == chess::Piece::WHITEPAWN) ||
            ((file & 0xfffffff8) && pos.at(chess::Square(chess::File(file + 1), y)) == chess::Piece::WHITEPAWN))
            return 0;
    }

    return (file && (rank & 0xfffffffe) && pos.at(square - 17) == chess::Piece::BLACKPAWN) ||
           ((file & 0xfffffff8) && (rank & 0xfffffffe) && pos.at(square - 15) == chess::Piece::BLACKPAWN) ||
           (rank && pos.at(square - 8) == chess::Piece::BLACKPAWN);
}
inline int supported(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, supported);
    if (pos.at(square) != chess::Piece::WHITEPAWN)
        return 0;
    return (pos.at(square + 7) == chess::Piece::WHITEPAWN) + (pos.at(square + 9) == chess::Piece::WHITEPAWN);
}
inline int phalanx(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, phalanx);
    if (pos.at(square) != chess::Piece::WHITEPAWN)
        return 0;
    if (pos.at(square - 1) == chess::Piece::WHITEPAWN || pos.at(square + 1) == chess::Piece::WHITEPAWN)
        return 1;
    return 0;
}
inline int connected(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, connected);
    return supported(pos,square)||phalanx(pos, square);
}
int opposed(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, opposed);
    if (pos.at(square) != chess::Piece::WHITEPAWN)
        return 0;

    chess::Bitboard bb(square.rank()),
	    	    bp=pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK);
    return (bb&bp)!=0;
}

inline int weak_unopposed_pawn(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, weak_unopposed_pawn);
    return (!opposed(pos, square)) && (isolated(pos, square) || backward(pos, square));
}
int connected_bonus(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, connected_bonus);
    if (!connected(pos, square))
        return 0;
    std::vector<int> seed = {0, 7, 8, 12, 29, 48, 86};
    int op = opposed(pos, square), ph = phalanx(pos, square), su = supported(pos, square), bl = pos.at(square - 1) == chess::Piece::WHITEPAWN, r = square.rank();
    if (r < 2 || r > 7)
        return 0;
    return seed[r - 1] * (2 + ph - op) + 21 * su;
}
int doubled(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, doubled);
    if (pos.at(square) != chess::Piece::WHITEPAWN)
        return 0;

    if (pos.at(square + 8) != chess::Piece::WHITEPAWN ||
        pos.at(square + 15) == chess::Piece::WHITEPAWN ||
        pos.at(square + 17) == chess::Piece::WHITEPAWN)
        return 0;
    return 1;
}
int blocked(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, blocked);
    if (pos.at(square) != chess::Piece::WHITEPAWN)
        return 0;

    int rank = square.rank();
    if (rank != 2 && rank != 3)
        return 0;

    if (pos.at(square - 8) != chess::Piece::BLACKPAWN)
        return 0;

    return 4 - rank;
}
int pawn_attacks_span(const chess::Position &pos, const chess::Square &square)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, pawn_attacks_span);

    chess::Position pos2 = colorflip(pos); // Assuming `colorflip` creates a Position with flipped colors
    for (int y = 0; y < square.rank(); y++)
    {
	chess::Rank br=chess::Rank::back_rank(y, chess::Color::WHITE);
	chess::Square w=chess::Square(square.file(), y+1),
		      w2=chess::Square(square.file(), y),
		      w3=chess::Square(square.file(), br);
	bool p1=(y==square.rank()-1);
	
        if (pos.at(w2 - 1) == chess::Piece::BLACKPAWN &&
            (p1 ||
             (pos.at(w - 1) != chess::Piece::WHITEPAWN &&
              !backward(pos2, w3 - 1))))
        {
            return 1;
        }

        if (pos.at(w2 + 1) == chess::Piece::BLACKPAWN &&
	   (p1 ||
             (pos.at(w+1) != chess::Piece::WHITEPAWN &&
              !backward(pos2, w3 + 1))))
        {
            return 1;
        }
    }

    return 0;
}

int outpost_square(const chess::Position &pos, const chess::Square &square)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, outpost_square);
    if (square.rank() != chess::Rank::RANK_5)
        return 0;

    if (pos.at(square + 15) != chess::Piece::WHITEPAWN &&
        pos.at(square + 17) != chess::Piece::WHITEPAWN)
        return 0;

    if (pawn_attacks_span(pos, square))
        return 0;

    return 1;
}

inline int pawns_mg(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, pawns_mg);
    int v = 0;
    if (doubled_isolated(pos, square))
        v -= 11;
    else if (isolated(pos, square))
        v -= 5;
    else if (backward(pos, square))
        v -= 9;
    v -= doubled(pos, square) * 11;
    v += connected(pos, square) ? connected_bonus(pos, square) : 0;
    v -= 13 * weak_unopposed_pawn(pos, square);
    v += std::vector<int>{0, -11, -3}[blocked(pos, square)];
    return v;
}
int reachable_outpost(const chess::Position &pos, chess::Square square)
{
    chess::Piece piece = pos.at(square);

    int v = 0;
    bool isKnight = (piece == chess::Piece::WHITEKNIGHT);

    for (int x = 0; x < 8; x++)
    {
        for (int y = 2; y < 5; y++)
        {
            chess::Square target = chess::Square(chess::File(x), y);
            if (pos.at(target) != chess::Piece::NONE)
                continue;

            chess::Bitboard attacks = isKnight
                                   ? chess::attacks::knight(target)
                                   : chess::attacks::bishop(target, pos.occ());

            if ((attacks & (1ULL << square.index())) && outpost_square(pos, target))
            {
                v = std::max(v, (pos.at(target + 15) == chess::Piece::WHITEPAWN ||
                                 pos.at(target + 17) == chess::Piece::WHITEPAWN) +
                                    1);
            }
        }
    }
    return v;
}

int outpost_total(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, outpost_total);

    chess::Piece piece = pos.at(square);
    if (piece != chess::Piece::WHITEKNIGHT && piece != chess::Piece::WHITEBISHOP)
        return 0;

    bool knight = (piece == chess::Piece::WHITEKNIGHT);
    if (outpost_square(pos, square))
    {
        return knight || reachable_outpost(pos, square);
    }

    if (knight && square.file() < chess::File::FILE_B || square.file() > chess::File::FILE_G)
    {
        bool enemy_attacks = false;
        int same_side_control = 0;
        chess::Bitboard bb = pos.us(chess::Color::BLACK);
        while (bb)
        {
            chess::Square sq = bb.pop();
            if ((std::abs(square.file() - sq.file()) == 2 && std::abs(square.rank() - sq.rank()) == 1) ||
                (std::abs(square.file() - sq.file()) == 1 && std::abs(square.rank() - sq.rank()) == 2))
            {
                enemy_attacks = true;
            }

            if ((sq.file() < chess::File::FILE_E && square.file() < chess::File::FILE_E) ||
                (sq.file() >= chess::File::FILE_E && square.file() >= chess::File::FILE_E))
            {
                same_side_control++;
            }
        }

        if (!enemy_attacks && same_side_control <= 1)
            return 2;
    }

    return knight ? 4 : 3;
}
inline int minor_behind_pawn(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, minor_behind_pawn);
    chess::Piece piece = pos.at(square);
    if (piece != chess::Piece::WHITEKNIGHT && piece != chess::Piece::WHITEBISHOP)
        return 0;
    if (pos.at(square - 8).type() != chess::PieceType::PAWN)
        return 0;
    return 1;
}
int bishop_pawns(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, bishop_pawns);
    if (pos.at(square) != chess::Piece::WHITEBISHOP)
        return 0;
    int x = square.file();

    int c = (x + (int)square.rank()) & 1, v = 0, _blocked = 0;
    chess::Bitboard bb = pos.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    while (bb)
    {
        chess::Square sq(bb.pop());
        chess::Piece piece = pos.at(sq);
        if (c == (int(sq.file()) + int(sq.rank())) & 1)
            v++;
        if (x > 1 && x < 6 && pos.at(sq - 8) != chess::Piece::NONE)
            _blocked++;
    }
    return v * (_blocked + (bool)(chess::attacks::pawn(pos.at(square).color(), square).count()));
}
inline int bishop_xray_pawns(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, bishop_xray_pawns);
    if (pos.at(square) != chess::Piece::WHITEBISHOP)
        return 0;
    int count = 0;
    chess::Bitboard bb = pos.pieces(chess::PieceType::PAWN, chess::Color::BLACK) & 0x8142241818244281;
    return bb.count();
}
inline int rook_on_queen_file(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, rook_on_queen_file);
    if (pos.at(square) != chess::Piece::WHITEROOK)
        return 0;
    return !pos.pieces(chess::PieceType::QUEEN) & chess::Bitboard(chess::File(square.file())).empty();
}

int king_ring(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, king_ring);

    int x = static_cast<int>(square.file());
    int y = static_cast<int>(square.rank());

    // Precompute the white king's position
    chess::Square whiteKingSq = pos.kingSq(chess::Color::WHITE);

    // Early exit if the white king is within distance 2
    if (chess::Square::value_distance(square, whiteKingSq) <= 2)
        return 1;

    // Check if square is defended by black pawns
    bool leftPawn = (square > 8 && pos.at(square - 17) == chess::Piece::BLACKPAWN);
    bool rightPawn = ((1 << square.index()) & 0xfcfcfcfcfc7dfc) && pos.at(square - 15) == chess::Piece::BLACKPAWN;

    return leftPawn && rightPawn;
}

inline int rook_on_king_ring(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, rook_on_king_ring);
    if (pos.at(square) != chess::Piece::WHITEROOK)
        return 0;
    if (pos.inCheck() * (pos.sideToMove() == chess::Color::WHITE ? 1 : -1) > 0)
        return 0;
    for (int y = 0; y < 8; y++)
    {
        if (king_ring(pos, chess::Square(square.file(), y)))
            return 1;
    }
    return 0;
}
int bishop_on_king_ring(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ)
{
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, bishop_on_king_ring);
    if (pos.at(square) != chess::Piece::WHITEBISHOP||
        pos.inCheck())
        return 0;
    for (int i = 0; i < 4; i++)
    {
        chess::File ix = ((i > 1) * 2 - 1) + square.file();
        chess::Rank iy = ((i % 2 == 0) * 2 - 1) + square.rank();
        for (int d = 1;
             d < 8 ||
             (ix <= chess::File::FILE_H) ||
             (iy <= chess::Rank::RANK_8);
             d++, ix = ix + d, iy = iy + d)
        {
            if (king_ring(pos, chess::Square(ix, iy)))
                return 1;
            if (pos.at(chess::Square(ix, iy)).type() != chess::PieceType::PAWN) break;
        }
    }
    return 0;
}
int rook_on_file(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ){
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, rook_on_file);
    if (pos.at(square) != chess::Piece::WHITEROOK)
        return 0;
    int open=1;
    for (int i = 0; i < 8; i++)
    {
        const chess::Piece p=pos.at(chess::Square(square.file(), i));
        if (p == chess::Piece::WHITEPAWN) return 0;
        if (p == chess::Piece::BLACKPAWN) open = 0;
    }
    return open+1;
}
class MoveGenHook: chess::movegen{
public:
    template <Color::underlying c, PieceType::underlying pt>
    [[nodiscard]] static Bitboard ppinMask(const Board &board, Square sq, Bitboard occ_enemy, Bitboard occ_us) noexcept{
pinMask<c,pt>(board, sq,occ_enemy,occ_us);
}
}
int blockers_for_king(const chess::Position& pos, const chess::Square &square = chess::Square::underlying::NO_SQ){
    if (square == chess::Square::underlying::NO_SQ)
	return sum(pos, blockers_for_king);
    if (pinned_direction(colorflip(pos),
			 chess::Square(square.file(), 7-square.rank())
	return 1;
}
int mobility_area(const chess::Position& pos, const chess::Square &square = chess::Square::underlying::NO_SQ){
    if (square == chess::Square::underlying::NO_SQ)
	return sum(pos, mobility_area);
    chess::PieceType b=pos.at(square).type();
    if (b==chess::Piece::WHITEQUEEN&&
        b==chess::Piece::WHITEKING)
	return 0;
    if (pos.at(square-9)==chess::Piece::BLACKPAWN||
	pos.at(square+7)==chess::Piece::BLACKPAWN)
	return 0;
    if (b==chess::Piece::WHITEPAWN&&
	(square.rank()<4||pos.at(square-1)!=chess::Piece::NONE))
	return 0;
    if (blockers_for_king(colorflip(pos),
			  chess::Square(square.file(), 7-square.rank()))
	return 0;
    return 1;
}
int mobility(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ){
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, mobility);
    int v=0;
    chess::PieceType b=pos.at(square).type();
    if (b!=chess::PieceType::NONE&& //most likely
        b!=chess::PieceType::PAWN&&
        b!=chess::PieceType::KING) //definitely, but only 2
        return 0;
    for (int i=0;i<64;i++){
	
    }
}
int trapped_rook(const chess::Position &pos, const chess::Square &square = chess::Square::underlying::NO_SQ){
    if (square == chess::Square::underlying::NO_SQ)
        return sum(pos, trapped_rook);
    
    if (pos.at(square) != chess::Piece::WHITEROOK||
        rook_on_file(pos, square)||
        mobility(pos, square)>3)return 0;
}
int pieces_mg(const chess::Position &pos)
{
    int v = 0;
    chess::Bitboard pieces = pos.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE) |
                             pos.pieces(chess::PieceType::BISHOP, chess::Color::WHITE) |
                             pos.pieces(chess::PieceType::ROOK, chess::Color::WHITE) |
                             pos.pieces(chess::PieceType::QUEEN, chess::Color::WHITE);

    while (pieces)
    {
        chess::Square square(pieces.pop()); // Get and remove the LSB
        v += std::array<int, 5>{0, 31, -7, 30, 56}[outpost_total(pos, square)];
        v += 18 * minor_behind_pawn(pos, square);
        v -= 3 * bishop_pawns(pos, square);
        v -= 4 * bishop_xray_pawns(pos, square);
        v += 6 * rook_on_queen_file(pos, square);
        v += 16 * rook_on_king_ring(pos, square);
        v += 24 * bishop_on_king_ring(pos, square);
        v += std::array<int, 3>{0, 19, 48}[rook_on_file(pos, square)];
        v -= trapped_rook(pos, square) * 55 * (pos.castlingRights().has(chess::Color::WHITE) + 1);
        v -= 56 * weak_queen(pos, square);
        v -= 2 * queen_infiltration(pos, square);
        v -= (pos.at(square) == chess::Piece::WHITEKNIGHT ? 8 : 6) * king_protector(pos, square);
        v += 45 * long_diagonal_bishop(pos, square);
    }

    return v;
}

int middle_game_evaluation(const chess::Position &pos, bool nowinnable = false)
{
    int v = 0;
    v += piece_value_mg(pos) - piece_value_mg(colorflip(pos));
    v += psqt_mg(pos) - psqt_mg(colorflip(pos));
    v += imbalance_total(pos);
    v += pawns_mg(pos) - pawns_mg(colorflip(pos));
    v += pieces_mg(pos) - pieces_mg(colorflip(pos));
    v += mobility_mg(pos) - mobility_mg(colorflip(pos));
    v += threats_mg(pos) - threats_mg(colorflip(pos));
    v += passed_mg(pos) - passed_mg(colorflip(pos));
    v += space(pos) - space(colorflip(pos));
    v += king_mg(pos) - king_mg(colorflip(pos));
    if (!nowinnable)
        v += winnable_total_mg(pos, v);
    return v;
}
int eval(const chess::Position &pos)
{
    int p = phase(pos);
    int eg = end_game_evaluation(pos);
    eg = eg * scale_factor(pos, eg) / 64;
    return ((((middle_game_evaluation(pos) * p + eg * (128 - p)) >> 7) & ~15) + tempo(pos)) * (100 - rule50(pos)) / 100;
}
