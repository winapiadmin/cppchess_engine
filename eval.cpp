#include "eval.h"


int eval(chess::Board &board){
	//Game over
	if (board.inCheck()&&board.isGameOver().first!=chess::GameResultReason::NONE)return MAX;
	else if (board.isGameOver().first!=chess::GameResultReason::NONE)return 0;
	int eval=0;
	//Material
	eval+=Pawn*board.pieces(chess::PieceType::underlying::PAWN,chess::Color::underlying::WHITE).count();
	eval+=Knight*board.pieces(chess::PieceType::underlying::KNIGHT,chess::Color::underlying::WHITE).count();
	eval+=Bishop*board.pieces(chess::PieceType::underlying::BISHOP,chess::Color::underlying::WHITE).count();
	eval+=Rook*board.pieces(chess::PieceType::underlying::ROOK,chess::Color::underlying::WHITE).count();
	eval+=Queen*board.pieces(chess::PieceType::underlying::QUEEN,chess::Color::underlying::WHITE).count();
	eval-=Pawn*board.pieces(chess::PieceType::underlying::PAWN,chess::Color::underlying::BLACK).count();
	eval-=Knight*board.pieces(chess::PieceType::underlying::KNIGHT,chess::Color::underlying::BLACK).count();
	eval-=Bishop*board.pieces(chess::PieceType::underlying::BISHOP,chess::Color::underlying::BLACK).count();
	eval-=Rook*board.pieces(chess::PieceType::underlying::ROOK,chess::Color::underlying::BLACK).count();
	eval-=Queen*board.pieces(chess::PieceType::underlying::QUEEN,chess::Color::underlying::BLACK).count();
	//Pawns
	eval -= isolated(board);
	eval -= dblisolated(board);
	eval -= weaks(board);
    eval -= blockage(board);
    eval += pawnIslands(board);
    eval -= holes(board);
    eval += pawnRace(board);
    eval -= underpromote(board);
    eval -= weakness(board);
    eval += pawnShield(board);
    eval += pawnStorm(board);
    eval += pawnLevers(board);
    eval += outpost(board);
    eval -= evaluatePawnRams(board);
    eval += evaluateUnfreePawns(board);
    eval += evaluateOpenPawns(board);
    eval -= evaluateBadBishops(board);
    eval += evaluateFianchetto(board);
    eval -= evaluateTrappedPieces(board);
    eval -= evaluateKnightForks(board);
    eval += evaluateRooksOnFiles(board);
    eval += evaluateKingSafety(board);
    eval += evaluateKingPawnTropism(board);
    eval += evaluateKingMobility(board);
    eval += evaluateSpaceControl(board);
    eval -= evaluateTactics(board);
    board.makeNullMove();
    eval -= -isolated(board);
	eval -= -dblisolated(board);
	eval -= -weaks(board);
    eval -= -blockage(board);
    eval += -pawnIslands(board);
    eval -= -holes(board);
    eval += -pawnRace(board);
    eval -= -underpromote(board);
    eval -= -weakness(board);
    eval += -pawnShield(board);
    eval += -pawnStorm(board);
    eval += -pawnLevers(board);
    eval += -outpost(board);
    eval -= -evaluatePawnRams(board);
    eval += -evaluateUnfreePawns(board);
    eval += -evaluateOpenPawns(board);
    eval -= -evaluateBadBishops(board);
    eval += -evaluateFianchetto(board);
    eval -= -evaluateTrappedPieces(board);
    eval -= -evaluateKnightForks(board);
    eval += -evaluateRooksOnFiles(board);
    eval += -evaluateKingSafety(board);
    eval += -evaluateKingPawnTropism(board);
    eval += -evaluateKingMobility(board);
    eval += -evaluateSpaceControl(board);
    eval -= -evaluateTactics(board);
    board.unmakeNullMove();
	return eval;
}
