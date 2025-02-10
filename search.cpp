#include "search.h"
unsigned long long nodes=0;
inline int piece_value(chess::PieceType type){
	switch (type.internal()){
		case chess::PieceType::PAWN: return Pawn;
		case chess::PieceType::KNIGHT: return Pawn;
		case chess::PieceType::BISHOP: return Pawn;
		case chess::PieceType::ROOK: return Pawn;
		case chess::PieceType::QUEEN: return Pawn;
		default: return 0;
	}
	return 0;
}
chess::Movelist order_moves(const chess::Board board,bool capturesonly=false) {
    chess::Movelist capture_moves, moves, quiet_moves;
    chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(capture_moves, board);
    chess::movegen::legalmoves<chess::movegen::MoveGenType::QUIET>(quiet_moves, board);
    // Create a vector of pairs from the unordered_map
    std::vector<std::pair<chess::Move, int>> vec;
    for (int i = 0; i < capture_moves.size(); i++) {
    	int score=0;
    	const auto move=capture_moves[i];
    	//definitely capture
    	const auto from=board.at(move.from()).type(),to=board.at(move.to()).type();
    	if (from<to)
    		score=10*piece_value(from)-piece_value(to);
    	if (move.promotionType()!=chess::PieceType::underlying::NONE)
    		score+=10+piece_value(move.promotionType());
    	if (board.isAttacked(move.to(), ~board.sideToMove()))
    		score-=piece_value(from);
    	vec.push_back({move,score});
    }
    if (!capturesonly)
		for (int i = 0; i < quiet_moves.size(); i++) {
    		int score=0;
    		const auto move=quiet_moves[i];
    		const auto from=board.at(move.from()).type();
    		if (move.promotionType()!=chess::PieceType::underlying::NONE)
    			score+=10+piece_value(move.promotionType());
    		if (board.isAttacked(move.to(), ~board.sideToMove()))
    			score-=piece_value(from);
    		vec.push_back({move,score});
    	}
    // Sort the vector by the second element (the value)
    std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second < b.second; // Sort in ascending order
    });
    for (std::pair<chess::Move,int> p:vec)moves.add(p.first);
    return moves;
}
int calculateExtensions(const chess::Board &board){
	int exts=0;
	if (board.inCheck())exts++;
	return exts;	
}
int searchcaptures(chess::Board board, int alpha, int beta) {
    if (board.isGameOver().first!=chess::GameResultReason::NONE) {
        return eval(board);
    }
    int stand_pat=eval(board),best_value=stand_pat;
    if (stand_pat>=beta)return stand_pat;
   	alpha=std::max(stand_pat,alpha);
    // Apply move ordering heuristics
    chess::Movelist moves=order_moves(board,true);

    int score;
    for (int i = 0; i < moves.size(); ++i) {
        const auto move = moves[i];
        board.makeMove(move);
        nodes++;
        score = -searchcaptures(board, -beta, -alpha);
        if (std::abs(score)>=MAX_MATE)score=(score<0?-1:1)*MATE(abs(score)+1);
        board.unmakeMove(move);
        if (score>=beta) {
            return beta; // Beta cutoff
        }
        alpha = std::max(alpha, score);
    }

    return alpha;
}
int search(chess::Board board, int depth, int alpha, int beta) {
    if (depth == 0) {
        return searchcaptures(board,alpha,beta);
    }
    if (board.isGameOver().first!=chess::GameResultReason::NONE)return eval(board);
    // Apply move ordering heuristics
    chess::Movelist moves=order_moves(board);
    
    int score;
    for (int i = 0; i < moves.size(); ++i) {
        const auto move = moves[i];
        board.makeMove(move);
        nodes++;
        score = -search(board, depth - 1 + calculateExtensions(board), -beta, -alpha);
        if (std::abs(score)>=MAX_MATE)score=(score<0?-1:1)*MATE(abs(score)+1);
        board.pop();
        if (beta<=alpha) {
            return alpha; // Beta cutoff
        }
        if (score>=beta) {
            return beta; // Beta cutoff
        }
        alpha = std::max(alpha, score);
    }

    return alpha;
}

