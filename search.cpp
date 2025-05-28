#include "search.hpp"

#include <cmath>
#include <cstring>  // for memmove, memcpy
#include <iostream>

#include "move_ordering.hpp"

namespace search {
constexpr int LMR_MOVE_COUNT_THRESHOLD = 4;
constexpr int LMR_AGGRESSIVE_THRESHOLD = 6;
PV pv;
TranspositionTable tt(25);  // 25MB TT
std::atomic<bool> stop_requested = false;
auto time_buffer = std::chrono::milliseconds(50);

int seldepth = 0;

bool check_time() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    if (elapsed + time_buffer >= time_limit) {
        stop_requested.store(true);
        return true;
    }
    return false;
}

int16_t quiescence(chess::Position &board, int16_t alpha, int16_t beta, int ply) {
    nodes++;
    seldepth = std::max(seldepth, ply);
    int16_t stand_pat = eval(board);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    if (moves.empty()) return board.inCheck() ? -MATE(ply) : stand_pat;

    move_ordering::moveOrder(board, moves, ply, chess::Move());

    const int16_t futility_margin = 200;
    const int16_t promotion_bonus = 200;

    for (const auto &move : moves) {
        bool givesCheck = board.givesCheck(move) != chess::CheckType::NO_CHECK;
        if (!(board.isCapture(move) || givesCheck || (move.typeOf() & chess::Move::PROMOTION))) continue;

        if (move_ordering::see(board, move.to()) < 0) continue;

        if (stand_pat + futility_margin + (move.typeOf() == chess::Move::PROMOTION ? promotion_bonus : 0) < alpha &&
            !givesCheck)
            continue;

        board.makeMove(move);
        int16_t score = -quiescence(board, -beta, -alpha, ply + 1);
        board.pop();

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth, int ply, PV *pv, int numExt = 0) {
    if (stop_requested.load() || check_time()) {
        return eval(board);
    }

    auto hash = board.hash();
    TTEntry *entry = tt.probe(hash);
    chess::Move hash_move = entry ? entry->bestMove.move() : chess::Move::NULL_MOVE;

    if (entry) {
        int16_t score = entry->score();
        if (entry->flag() == TTFlag::LOWERBOUND && score >= beta) return score;
        if (entry->flag() == TTFlag::UPPERBOUND && score <= alpha) return score;

        if (entry->flag() == TTFlag::EXACT && entry->depth() >= depth) {
            memmove(&pv->argmove[1], pv->argmove, sizeof(chess::Move) * pv->cmove);
            pv->argmove[0] = hash_move.move();
            pv->cmove++;
            return score;
        }
    }

    if (depth <= 0) return quiescence(board, alpha, beta, ply);
    if (board.isRepetition(1)) return 0;
    bool in_check = board.inCheck();

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    if (moves.empty()) return in_check ? -MATE(ply) : 0;

    nodes++;
    if (pv->cmove && hash_move == chess::Move()) hash_move = pv->argmove[0];
    move_ordering::moveOrder(board, moves, ply, hash_move);

    int16_t best_score = -MATE(ply);
    chess::Move best_move;
    int moveCount = 0;
    for (const auto &move : moves) {
        PV child_pv;
        child_pv.clear();
        if (stop_requested.load() || check_time()) break;

        board.makeMove(move);
        int16_t score = -alphaBeta(board, -beta, -alpha, depth - 1, ply + 1, &child_pv, numExt);
        board.pop();

        moveCount++;

        if (score > best_score) {
            best_score = score;
            best_move = move;
            memmove(&pv->argmove[1], child_pv.argmove, sizeof(chess::Move) * child_pv.cmove);
            pv->argmove[0] = move.move();
            pv->cmove = child_pv.cmove + 1;  // set new length properly
        }

        if (best_score >= beta) break;  // Beta cutoff

        alpha = std::max(alpha, best_score);
    }

    TTFlag flag = best_score >= beta ? TTFlag::LOWERBOUND : (best_score <= alpha ? TTFlag::UPPERBOUND : TTFlag::EXACT);
    tt.store(hash, best_move, best_score, depth, flag);

    return best_score;
}

int16_t alphaBeta(chess::Position &board, int16_t alpha, int16_t beta, int depth) {
    stop_requested = false;
    PV local_pv;
    local_pv.clear();

    int16_t score = alphaBeta(board, alpha, beta, depth, 0, &local_pv);

    std::memcpy(pv.argmove, local_pv.argmove, sizeof(chess::Move) * local_pv.cmove);
    pv.cmove = local_pv.cmove;

    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time);

    std::cout << "info depth " << depth << " seldepth " << seldepth;

    if (std::abs(score) > MAX_SCORE_CP) {
        int m = MATE_DISTANCE(std::abs(score));
        std::cout << " score mate " << (score < 0 ? -m : m);
    } else {
        std::cout << " score cp " << score;
    }

    std::cout << " nodes " << nodes << " time " << elapsed.count() << " nps "
              << (elapsed.count() ? (nodes * 1000 / elapsed.count()) : 0);

    if (pv.cmove > 0) {
        std::cout << " pv";
        for (int i = 0; i < pv.cmove; ++i) {
            std::cout << " " << chess::uci::moveToUci(chess::Move(pv.argmove[i]), board.chess960());
        }
    }

    std::cout << std::endl;
    return score;
}

// Search function
void run_search(chess::Position board, int depth, unsigned time_limit_ms, bool infinite) {
    search::nodes = 0;
    search::tt.newSearch();
    search::start_time = std::chrono::high_resolution_clock::now();
    search::time_limit = std::chrono::milliseconds((int)(time_limit_ms * 0.3));
    search::PV stablePV;

    int prev_score = search::alphaBeta(board, -MATE(0), MATE(0), 0);
    for (int d = 1; (depth == -1 || d <= depth) && !search::stop_requested.load(); ++d) {
        int alpha = prev_score - UCIOptions::getInt("aspiration_window");
        int beta = prev_score + UCIOptions::getInt("aspiration_window");

        int16_t score = search::alphaBeta(board, alpha, beta, d);

        if (score <= alpha) {
            alpha = -MATE(0);
            beta = prev_score + UCIOptions::getInt("aspiration_window");
            score = search::alphaBeta(board, alpha, beta, d);
        } else if (score >= beta) {
            alpha = prev_score - UCIOptions::getInt("aspiration_window");
            beta = MATE(0);
            score = search::alphaBeta(board, alpha, beta, d);
        }
        if (!infinite && search::check_time()) break;
    }

    chess::Move best = search::pv.argmove[0];
    chess::Move ponder = search::pv.argmove[1];

    if (best != chess::Move()) {
        std::cout << "bestmove " << best;
        if (ponder != chess::Move()) std::cout << " ponder " << ponder;
        std::cout << std::endl;
    } else {
        std::cout << "bestmove (none)" << std::endl;
    }
}

}  // namespace search
