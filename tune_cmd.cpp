#include "tune_cmd.h"
#include "Weights.h"
#include "eval.h"
#include "tune.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <csv.hpp>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <position.h>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
namespace engine {

namespace {

double sigmoid(double cp, double K = 0.004) { return 1.0 / (1.0 + std::exp(-K * cp)); }

struct TuneParam {
    std::string name;
    int *value_ptr;
    int lower;
    int upper;
};

std::vector<TuneParam> collect_params() {
    std::vector<TuneParam> params;
    for (auto &e : Tune::get_list()) {
        if (auto entry = dynamic_cast<Tune::Entry<int> *>(e.get())) {
            auto bounds = entry->range(entry->value);
            params.push_back({ entry->name, &entry->value, bounds.first, bounds.second });
        }
    }
    return params;
}

struct TuneData {
    std::vector<char> fen_buf;
    std::vector<size_t> offsets;
    std::vector<double> results;

    size_t size() const { return results.size(); }

    void add(const std::string &fen, double result) {
        offsets.push_back(fen_buf.size());
        fen_buf.insert(fen_buf.end(), fen.begin(), fen.end());
        fen_buf.push_back('\0');
        results.push_back(result);
    }

    double result(size_t i) const { return results[i]; }
};

double eval_loss(const TuneData &data, const std::vector<size_t> &indices, int num_threads = 0) {
    if (indices.empty())
        return 0.0;
    if (num_threads <= 0)
        num_threads = (int)std::thread::hardware_concurrency();
    if (num_threads < 1)
        num_threads = 1;

    size_t n = indices.size();
    std::vector<double> partial(num_threads, 0.0);
    std::vector<std::thread> threads;

    auto worker = [&](int tid, size_t start, size_t end) {
        chess::Position pos;
        std::string fen;
        double sum = 0.0;
        for (size_t j = start; j < end; j++) {
            size_t idx = indices[j];
            fen.assign(data.fen_buf.data() + data.offsets[idx]);
            pos.set_fen(fen);
            Value score = eval::eval(pos);
            double diff = sigmoid(score) - data.result(idx);
            sum += diff * diff;
        }
        partial[tid] = sum;
    };

    size_t chunk = (n + num_threads - 1) / num_threads;
    for (int t = 0; t < num_threads; t++) {
        size_t start = t * chunk;
        size_t end = std::min(start + chunk, n);
        if (start >= n)
            break;
        threads.emplace_back(worker, t, start, end);
    }

    for (auto &t : threads)
        t.join();

    double loss = std::accumulate(partial.begin(), partial.end(), 0.0);
    return loss / n;
}

std::unordered_map<int *, int> build_addr_map(const std::vector<TuneParam> &params) {
    std::unordered_map<int *, int> map;
    map.reserve(params.size());
    for (int i = 0; i < (int)params.size(); i++)
        map[params[i].value_ptr] = i;
    return map;
}

// Compute passed-pawn mask for a single square and color
chess::Bitboard passed_pawn_mask(chess::Color c, chess::Square sq) {
    chess::Bitboard mask = 0;
    chess::File f = chess::file_of(sq);
    int startF = std::max(0, (int)f - 1);
    int endF = std::min(7, (int)f + 1);
    for (int adjF = startF; adjF <= endF; adjF++) {
        if (c == chess::WHITE) {
            for (int r = (int)chess::rank_of(sq) + 1; r <= 7; r++)
                mask |= chess::attacks::MASK_FILE[adjF] & chess::attacks::MASK_RANK[r];
        } else {
            for (int r = (int)chess::rank_of(sq) - 1; r >= 0; r--)
                mask |= chess::attacks::MASK_FILE[adjF] & chess::attacks::MASK_RANK[r];
        }
    }
    return mask;
}

static constexpr int KnightPhase = 1;
static constexpr int BishopPhase = 1;
static constexpr int RookPhase = 2;
static constexpr int QueenPhase = 4;
static constexpr int TotalPhase = KnightPhase * 4 + BishopPhase * 4 + RookPhase * 4 + QueenPhase * 2;

int compute_game_phase(const chess::Position &board) {
    int phase = 0;
    chess::Bitboard occ = board.occ();
    while (occ) {
        chess::Square sq = chess::Square(chess::pop_lsb(occ));
        auto pt = chess::piece_of(board.at(sq));
        if (pt == chess::KNIGHT)
            phase += KnightPhase;
        else if (pt == chess::BISHOP)
            phase += BishopPhase;
        else if (pt == chess::ROOK)
            phase += RookPhase;
        else if (pt == chess::QUEEN)
            phase += QueenPhase;
    }
    return (phase * 256 + TotalPhase / 2) / TotalPhase;
}

void accumulate_gradient(const chess::Position &board,
                         double common_factor,
                         const std::unordered_map<int *, int> &addr_to_idx,
                         double phase_mg,
                         double phase_eg,
                         double stm_sign,
                         std::vector<double> &gradient) {
    using namespace chess;
    using namespace engine::eval;

    Bitboard pawnBB[2] = { board.pieces(PAWN, WHITE), board.pieces(PAWN, BLACK) };
    Bitboard pawnAtks[2] = { attacks::pawn<WHITE>(pawnBB[WHITE]), attacks::pawn<BLACK>(pawnBB[BLACK]) };

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Square qStart = c == WHITE ? SQ_D1 : SQ_D8;
        if (!(board.pieces(QUEEN, c) & (1ULL << qStart))) {
            Bitboard backRank = c == WHITE ? attacks::MASK_RANK[0] : attacks::MASK_RANK[7];
            int undeveloped = popcount((board.pieces(KNIGHT, c) | board.pieces(BISHOP, c)) & backRank);
            if (undeveloped >= 2) {
                auto it = addr_to_idx.find(&earlyQueenPenalty);
                if (it != addr_to_idx.end())
                    gradient[it->second] += common_factor * (s * stm_sign) * (-undeveloped);
            }
        }
    }

    {
        Bitboard pinMask = board.pin_mask();
        Bitboard occ = board.occ(), occ2 = occ;

        while (occ) {
            Square sq = Square(pop_lsb(occ));
            auto p = board.at(sq);
            Color pc = color_of(p);
            double _sign = (pc == WHITE) ? 1.0 : -1.0;
            double eff = _sign * stm_sign;
            Square _sq = _sign == 1.0 ? sq : square_mirror(sq);
            auto pt = piece_of(p);
            if (pt == NO_PIECE_TYPE)
                continue;

            {
                auto *mg_arr = mgPst[pt];
                auto *eg_arr = egPst[pt];
                auto it_mg = addr_to_idx.find(&mg_arr[_sq]);
                auto it_eg = addr_to_idx.find(&eg_arr[_sq]);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff * phase_mg;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff * phase_eg;
            }

            auto add_mat = [&](auto *addr) {
                auto it = addr_to_idx.find(addr);
                if (it != addr_to_idx.end()) {
                    gradient[it->second] += common_factor * eff * phase_mg;
                    gradient[it->second] += common_factor * eff * phase_eg;
                }
            };
            switch (pt) {
            case PAWN:
                add_mat(&PawnValue);
                break;
            case KNIGHT:
                add_mat(&KnightValue);
                break;
            case BISHOP:
                add_mat(&BishopValue);
                break;
            case ROOK:
                add_mat(&RookValue);
                break;
            case QUEEN:
                add_mat(&QueenValue);
                break;
            default:
                break;
            }

            Bitboard atks = 0;
            if (pt == KNIGHT)
                atks = chess::attacks::knight(sq);
            else if (pt == BISHOP)
                atks = chess::attacks::bishop(sq, occ2);
            else if (pt == ROOK)
                atks = chess::attacks::rook(sq, occ2);
            else if (pt == QUEEN)
                atks = chess::attacks::queen(sq, occ2);
            atks &= ~board.us(pc);
            int mobility = popcount(atks);
            int cmob = std::min(mobility, 7);
            if ((1ULL << sq) & pinMask)
                cmob = std::min(cmob / 4, 7);
            {
                auto it_mg = addr_to_idx.find(&mgMobilityCnt[pt][cmob]);
                auto it_eg = addr_to_idx.find(&egMobilityCnt[pt][cmob]);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff * phase_mg;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff * phase_eg;
            }

            if (pt != PAWN && pt != KING) {
                int kd = square_distance(sq, board.kingSq(~pc));
                double coeff = 7 - kd;
                auto it_mg = addr_to_idx.find(&kingTropismMg[pt]);
                auto it_eg = addr_to_idx.find(&kingTropismEg[pt]);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff * coeff * phase_mg;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff * coeff * phase_eg;
            }

            if (pt != PAWN && pt != KING) {
                int kdist = square_distance(sq, board.kingSq(pc));
                int ki = std::min(kdist, 5);
                double coeff = 1.0 / (1 + ki);
                auto it_mg = addr_to_idx.find(&kingProtector[pt][0]);
                auto it_eg = addr_to_idx.find(&kingProtector[pt][1]);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff * coeff * phase_mg;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff * coeff * phase_eg;
            }

            if (pt == KNIGHT || pt == BISHOP) {
                Rank r = rank_of(sq);
                bool onOutpost = (pc == WHITE && r >= RANK_5) || (pc == BLACK && r <= RANK_4);
                if (onOutpost && !(pawnAtks[~pc] & (1ULL << sq))) {
                    bool defended = (pawnAtks[pc] & (1ULL << sq)) != 0;
                    int idx = defended ? 1 : 0;
                    auto *arr = (pt == KNIGHT) ? outpostBonusKnight : outpostBonusBishop;
                    auto it = addr_to_idx.find(&arr[idx]);
                    if (it != addr_to_idx.end()) {
                        gradient[it->second] += common_factor * eff * phase_mg;
                        gradient[it->second] += common_factor * eff * phase_eg;
                    }
                }
            }
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        if (board.count(BISHOP, c) >= 2) {
            double eff = s * stm_sign;
            auto it_mg = addr_to_idx.find(&bishopPairMg);
            auto it_eg = addr_to_idx.find(&bishopPairEg);
            if (it_mg != addr_to_idx.end())
                gradient[it_mg->second] += common_factor * eff * phase_mg;
            if (it_eg != addr_to_idx.end())
                gradient[it_eg->second] += common_factor * eff * phase_eg;
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Square fSq[2] = { relative_square(c, SQ_B2), relative_square(c, SQ_G2) };
        Square pSq[2] = { relative_square(c, SQ_B3), relative_square(c, SQ_G3) };
        for (int i = 0; i < 2; i++) {
            if (board.at<PieceType>(fSq[i]) == BISHOP && board.at<Color>(fSq[i]) == c && board.at<PieceType>(pSq[i]) == PAWN &&
                board.at<Color>(pSq[i]) == c) {
                auto it = addr_to_idx.find(&fianchettoBonus);
                if (it != addr_to_idx.end())
                    gradient[it->second] += common_factor * (s * stm_sign);
            }
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Square trapSq[2] = { relative_square(c, SQ_A2), relative_square(c, SQ_H2) };
        Square kingAdj[2] = { relative_square(c, SQ_B1), relative_square(c, SQ_G1) };
        for (int i = 0; i < 2; i++) {
            if (board.at<PieceType>(trapSq[i]) == BISHOP && board.at<Color>(trapSq[i]) == c && board.kingSq(c) == kingAdj[i]) {
                auto it = addr_to_idx.find(&trappedBishopPenalty);
                if (it != addr_to_idx.end())
                    gradient[it->second] += common_factor * (-s * stm_sign);
            }
        }
    }
    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Color opp = ~c;
        Square sqs[2] = { relative_square(c, SQ_A7), relative_square(c, SQ_H7) };
        Square pSqs[2] = { relative_square(c, SQ_B6), relative_square(c, SQ_G6) };
        for (int i = 0; i < 2; i++) {
            if (board.at<PieceType>(sqs[i]) == BISHOP && board.at<Color>(sqs[i]) == c) {
                if (board.at<PieceType>(pSqs[i]) == PAWN && board.at<Color>(pSqs[i]) == opp &&
                    !board.is_attacked_by(c, sqs[i])) {
                    auto it = addr_to_idx.find(&trappedBishopPenalty);
                    if (it != addr_to_idx.end())
                        gradient[it->second] += common_factor * (-s * stm_sign);
                }
            }
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Bitboard rooks = board.pieces(ROOK, c);
        while (rooks) {
            Square sq = Square(pop_lsb(rooks));
            File f = file_of(sq);
            Bitboard fileMask = attacks::MASK_FILE[f];
            bool hasOwn = (board.pieces(PAWN, c) & fileMask) != 0;
            bool hasEnemy = (board.pieces(PAWN, ~c) & fileMask) != 0;
            double eff = s * stm_sign;
            if (!hasOwn && !hasEnemy) {
                auto it_mg = addr_to_idx.find(&rookOpenFileMg);
                auto it_eg = addr_to_idx.find(&rookOpenFileEg);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff * phase_mg;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff * phase_eg;
            } else if (!hasOwn) {
                auto it_mg = addr_to_idx.find(&rookSemiOpenFileMg);
                auto it_eg = addr_to_idx.find(&rookSemiOpenFileEg);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff * phase_mg;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff * phase_eg;
            }
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Bitboard rooks = board.pieces(ROOK, c);
        while (rooks) {
            Square sq = Square(pop_lsb(rooks));
            Rank r7 = c == WHITE ? RANK_7 : RANK_2;
            if (rank_of(sq) != r7)
                continue;
            Bitboard ep = board.pieces(PAWN, ~c) & attacks::MASK_RANK[r7];
            int cnt = 0;
            Bitboard tmp = ep;
            while (tmp) {
                Square psq = Square(pop_lsb(tmp));
                if (!board.is_attacked_by(~c, psq))
                    cnt++;
            }
            if (cnt >= 2) {
                double eff = s * stm_sign;
                auto it = addr_to_idx.find(&rookOnSeventhBonus);
                if (it != addr_to_idx.end()) {
                    gradient[it->second] += common_factor * eff * 0.5 * phase_mg;
                    gradient[it->second] += common_factor * eff * phase_eg;
                }
            }
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        double eff = s * stm_sign;
        Bitboard pawns = pawnBB[c];
        Bitboard enemyPawns = pawnBB[~c];
        bool fileHasPawn[8] = { false };
        int fileCount[8] = { 0 };
        Bitboard tmp = pawns;
        while (tmp) {
            Square sq = Square(pop_lsb(tmp));
            fileCount[file_of(sq)]++;
            fileHasPawn[file_of(sq)] = true;
        }

        for (int f = 0; f < 8; f++)
            if (fileCount[f] >= 2) {
                double coeff = -(fileCount[f] - 1);
                auto it_mg = addr_to_idx.find(&doubledPawnMg);
                auto it_eg = addr_to_idx.find(&doubledPawnEg);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff * coeff;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff * coeff;
            }

        tmp = pawns;
        while (tmp) {
            Square sq = Square(pop_lsb(tmp));
            File f = file_of(sq);
            Rank relRank = relative_rank(c, sq);

            bool isolated = true;
            if (f > FILE_A && fileHasPawn[f - 1])
                isolated = false;
            if (f < FILE_H && fileHasPawn[f + 1])
                isolated = false;
            if (isolated) {
                auto it_mg = addr_to_idx.find(&isolatedPawnMg);
                auto it_eg = addr_to_idx.find(&isolatedPawnEg);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff * (-1.0);
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff * (-1.0);
            }

            if (relRank >= RANK_2 && (passed_pawn_mask(c, sq) & enemyPawns) == 0) {
                auto it_mg = addr_to_idx.find(&passedBonusMg[relRank]);
                auto it_eg = addr_to_idx.find(&passedBonusEg[relRank]);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff;
            }
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Bitboard pawns = pawnBB[c];
        while (pawns) {
            Square sq = Square(pop_lsb(pawns));
            Square forward = sq + pawn_push(c);
            if (forward >= SQ_A1 && forward <= SQ_H8 && (pawnBB[~c] & (1ULL << forward))) {
                auto it = addr_to_idx.find(&rammedPawnPenalty);
                if (it != addr_to_idx.end())
                    gradient[it->second] += common_factor * (-s * stm_sign);
            }
        }
    }

    {
        Bitboard centerMask = (1ULL << SQ_D4) | (1ULL << SQ_E4) | (1ULL << SQ_D5) | (1ULL << SQ_E5);
        int wc = popcount(pawnAtks[WHITE] & centerMask);
        int bc = popcount(pawnAtks[BLACK] & centerMask);
        auto it = addr_to_idx.find(&centerWeight);
        if (it != addr_to_idx.end())
            gradient[it->second] += common_factor * stm_sign * (wc - bc);
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Bitboard safe = ~pawnAtks[~c];
        Bitboard camp = c == WHITE ? (attacks::MASK_RANK[4] | attacks::MASK_RANK[5] | attacks::MASK_RANK[6])
                                   : (attacks::MASK_RANK[3] | attacks::MASK_RANK[2] | attacks::MASK_RANK[1]);
        int space = popcount(pawnAtks[c] & safe & camp);
        auto it = addr_to_idx.find(&spaceWeight);
        if (it != addr_to_idx.end())
            gradient[it->second] += common_factor * (s * stm_sign) * space;
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Square kingSq = board.kingSq(c);
        File kf = file_of(kingSq);
        Rank kr = rank_of(kingSq);
        Bitboard pawns = board.pieces(PAWN, c);
        int startF = std::max(0, (int)kf - 1);
        int endF = std::min(7, (int)kf + 1);
        double eff_base = s * stm_sign;
        for (int adjF = startF; adjF <= endF; adjF++) {
            Bitboard fileMask = attacks::MASK_FILE[adjF];
            auto check_rank = [&](int r) {
                if (!(pawns & (fileMask & attacks::MASK_RANK[r])))
                    return;
                double dist = (c == WHITE) ? (r - (int)kr - 1) : ((int)kr - r - 1);
                auto it_bmg = addr_to_idx.find(&kingShelterBaseMg);
                auto it_beg = addr_to_idx.find(&kingShelterBaseEg);
                auto it_dmg = addr_to_idx.find(&kingShelterDecayMg);
                auto it_deg = addr_to_idx.find(&kingShelterDecayEg);
                if (it_bmg != addr_to_idx.end())
                    gradient[it_bmg->second] += common_factor * eff_base * phase_mg;
                if (it_beg != addr_to_idx.end())
                    gradient[it_beg->second] += common_factor * eff_base * phase_eg;
                if (it_dmg != addr_to_idx.end())
                    gradient[it_dmg->second] += common_factor * eff_base * (-dist) * phase_mg;
                if (it_deg != addr_to_idx.end())
                    gradient[it_deg->second] += common_factor * eff_base * (-dist) * phase_eg;
            };
            if (c == WHITE) {
                for (int r = (int)kr + 1; r <= std::min(7, (int)kr + 3); r++)
                    check_rank(r);
            } else {
                for (int r = (int)kr - 1; r >= std::max(0, (int)kr - 3); r--)
                    check_rank(r);
            }
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        double eff = s * stm_sign;
        int oppCount = popcount(board.occ(~c));
        Square oppKing = board.kingSq(~c);
        File ef = file_of(oppKing);
        Rank er = rank_of(oppKing);
        int edgeDist = std::min(std::min((int)ef, 7 - (int)ef), std::min((int)er, 7 - (int)er));

        if (board.count(QUEEN, c) >= 1 && oppCount == 1) {
            Square myKing = board.kingSq(c);
            int kd = std::max(std::abs((int)file_of(myKing) - (int)file_of(oppKing)),
                              std::abs((int)rank_of(myKing) - (int)rank_of(oppKing)));
            auto it1 = addr_to_idx.find(&kqkDistWeight);
            auto it2 = addr_to_idx.find(&kqkEdgeWeight);
            if (it1 != addr_to_idx.end())
                gradient[it1->second] += common_factor * eff * (14 - kd);
            if (it2 != addr_to_idx.end())
                gradient[it2->second] += common_factor * eff * (7 - edgeDist);
        }

        if (board.count(ROOK, c) >= 1 && oppCount == 1 && board.count(QUEEN, c) == 0 && board.count(BISHOP, c) == 0 &&
            board.count(KNIGHT, c) == 0 && board.count(PAWN, c) == 0) {
            Square myKing = board.kingSq(c);
            int kd = std::max(std::abs((int)file_of(myKing) - (int)file_of(oppKing)),
                              std::abs((int)rank_of(myKing) - (int)rank_of(oppKing)));
            auto it1 = addr_to_idx.find(&krkDistWeight);
            auto it2 = addr_to_idx.find(&krkEdgeWeight);
            if (it1 != addr_to_idx.end())
                gradient[it1->second] += common_factor * eff * (14 - kd);
            if (it2 != addr_to_idx.end())
                gradient[it2->second] += common_factor * eff * (7 - edgeDist);
        }

        if (board.count(PAWN, c) >= 1 && oppCount == 1 && board.count(QUEEN, c) == 0 && board.count(ROOK, c) == 0 &&
            board.count(BISHOP, c) == 0 && board.count(KNIGHT, c) == 0) {
            Bitboard ps = pawnBB[c];
            while (ps) {
                Square psq = Square(pop_lsb(ps));
                Rank pr = relative_rank(c, psq);
                if (pr < RANK_4)
                    continue;
                File pf = file_of(psq);
                Square promSq = make_sq(pf, c == WHITE ? RANK_8 : RANK_1);
                Square myKing = board.kingSq(c);
                int pawnDist = (c == WHITE) ? (7 - (int)rank_of(psq)) : (int)rank_of(psq);
                int pkDist =
                    std::max(std::abs((int)file_of(oppKing) - (int)pf), std::abs((int)rank_of(oppKing) - (int)rank_of(promSq)));
                int mkDist = std::max(std::abs((int)file_of(myKing) - (int)file_of(psq)),
                                      std::abs((int)rank_of(myKing) - (int)rank_of(psq)));
                bool winning = pkDist > pawnDist || mkDist <= pawnDist + 1 ||
                               (pr >= RANK_6 && file_of(myKing) == pf &&
                                ((c == WHITE && rank_of(myKing) >= RANK_6) || (c == BLACK && rank_of(myKing) <= RANK_3)));
                if ((pf == FILE_A || pf == FILE_H) && oppKing == promSq)
                    winning = false;
                if (winning) {
                    auto it = addr_to_idx.find(&kpkWeight);
                    if (it != addr_to_idx.end())
                        gradient[it->second] += common_factor * eff * pawnDist;
                }
            }
        }

        Bitboard ourMat = board.occ(c) & ~board.pieces(PAWN) & ~board.pieces(KING);
        Bitboard theirMat = board.occ(~c) & ~board.pieces(PAWN) & ~board.pieces(KING);
        if (ourMat && !theirMat && oppCount <= 2) {
            Square myKing = board.kingSq(c);
            int kd = std::max(std::abs((int)file_of(myKing) - (int)file_of(oppKing)),
                              std::abs((int)rank_of(myKing) - (int)rank_of(oppKing)));
            auto it1 = addr_to_idx.find(&mopUpKingDistWeight);
            auto it2 = addr_to_idx.find(&mopUpEdgeDistWeight);
            if (it1 != addr_to_idx.end())
                gradient[it1->second] += common_factor * eff * (14 - kd);
            if (it2 != addr_to_idx.end())
                gradient[it2->second] += common_factor * eff * (7 - edgeDist);
        }
    }

    for (Color c : { WHITE, BLACK }) {
        double s = (c == WHITE) ? 1.0 : -1.0;
        Color opp = ~c;
        Bitboard ourPieces = board.occ(c) & ~board.pieces(PAWN) & ~board.pieces(KING);
        while (ourPieces) {
            Square sq = Square(pop_lsb(ourPieces));
            PieceType pt = piece_of(board.at(sq));
            bool attacked = board.is_attacked_by(opp, sq);
            if (!attacked)
                continue;
            bool defended = board.is_attacked_by(c, sq);
            Bitboard occ = board.occ();
            int attackers = popcount(board.attackers_mask(opp, sq, occ));
            int defenders = defended ? popcount(board.attackers_mask(c, sq, occ)) : 0;
            bool hanging = !defended;
            bool overloaded = defenders == 1 && attackers >= 2;
            bool attackedByMinor =
                (board.attackers_mask(opp, sq, occ) & (board.pieces(KNIGHT, opp) | board.pieces(BISHOP, opp))) != 0;
            bool attackedByRook = (board.attackers_mask(opp, sq, occ) & board.pieces(ROOK, opp)) != 0;

            double eff_threat = -s * stm_sign;

            if (hanging) {
                double mg_hf = (pt == QUEEN) ? 0.5 : 1.0;
                double eg_hf = (pt == QUEEN) ? 0.25 : 0.5;
                auto it = addr_to_idx.find(&hangingScore);
                if (it != addr_to_idx.end()) {
                    gradient[it->second] += common_factor * eff_threat * mg_hf * phase_mg;
                    gradient[it->second] += common_factor * eff_threat * eg_hf * phase_eg;
                }
            }

            if (overloaded) {
                auto it = addr_to_idx.find(&overloadScore);
                if (it != addr_to_idx.end()) {
                    gradient[it->second] += common_factor * eff_threat * phase_mg;
                    gradient[it->second] += common_factor * eff_threat * 0.5 * phase_eg;
                }
            }

            if (attackedByMinor) {
                auto it_mg = addr_to_idx.find(&threatByMinor[pt][0]);
                auto it_eg = addr_to_idx.find(&threatByMinor[pt][1]);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff_threat;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff_threat;
            }

            if (attackedByRook) {
                auto it_mg = addr_to_idx.find(&threatByRook[pt][0]);
                auto it_eg = addr_to_idx.find(&threatByRook[pt][1]);
                if (it_mg != addr_to_idx.end())
                    gradient[it_mg->second] += common_factor * eff_threat;
                if (it_eg != addr_to_idx.end())
                    gradient[it_eg->second] += common_factor * eff_threat;
            }

            Rank relRank = relative_rank(c, sq);
            auto it = addr_to_idx.find(&threatByRankScore);
            if (it != addr_to_idx.end()) {
                gradient[it->second] += common_factor * eff_threat * relRank * phase_mg;
                gradient[it->second] += common_factor * eff_threat * relRank * phase_eg;
            }
        }
    }

    {
        int pc[2][7] = { { 0 } };
        for (Color c : { WHITE, BLACK })
            for (PieceType pt : { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING })
                pc[c][pt] = board.count(pt, c);
        int ourM = pc[WHITE][KNIGHT] + pc[WHITE][BISHOP];
        int theirM = pc[BLACK][KNIGHT] + pc[BLACK][BISHOP];
        auto add_imb = [&](auto *addr, int coeff) {
            auto it = addr_to_idx.find(addr);
            if (it != addr_to_idx.end())
                gradient[it->second] += common_factor * stm_sign * coeff;
        };
        add_imb(&minorImWt, ourM - theirM);
        add_imb(&bishopImWt, pc[WHITE][BISHOP] - pc[BLACK][BISHOP]);
        add_imb(&rookImWt, pc[WHITE][ROOK] - pc[BLACK][ROOK]);
        add_imb(&queenImWt, pc[WHITE][QUEEN] - pc[BLACK][QUEEN]);
    }
}

void texel_tune(TuneData &all,
                const std::vector<size_t> &train_idx,
                const std::vector<size_t> &holdout_idx,
                std::vector<TuneParam> &params,
                int iterations,
                const std::string &weights_header) {
    auto addr_map = build_addr_map(params);
    int dim = (int)params.size();
    int num_threads = (int)std::thread::hardware_concurrency();
    if (num_threads < 1)
        num_threads = 1;
    size_t n = train_idx.size();

    double base_loss = eval_loss(all, train_idx);
    double base_hold_loss = eval_loss(all, holdout_idx);
    std::cout << "info string texel_tune: dim=" << dim << " positions=" << n << " initial_loss=" << base_loss * train_idx.size()
              << std::endl;

    double lr = 1.0;

    std::vector<double> best_x(dim);
    for (int i = 0; i < dim; i++)
        best_x[i] = *params[i].value_ptr;
    double best_loss = base_hold_loss;

    for (int iter = 0; iter < iterations; iter++) {
        auto t0 = std::chrono::steady_clock::now();

        std::vector<double> gradient(dim, 0.0);
        std::vector<std::thread> threads;
        std::vector<double> partial_loss(num_threads, 0.0);
        std::vector<std::vector<double>> partial_grad(num_threads, std::vector<double>(dim, 0.0));
        size_t chunk = (n + num_threads - 1) / num_threads;

        auto worker = [&](int tid, size_t start, size_t end) {
            chess::Position pos;
            std::string fen;
            double ploss = 0.0;
            auto &pg = partial_grad[tid];

            for (size_t j = start; j < end; j++) {
                size_t idx = train_idx[j];
                fen.assign(all.fen_buf.data() + all.offsets[idx]);
                pos.set_fen(fen);

                auto comp = eval::eval_components(pos);
                const int sign = pos.side_to_move() == chess::WHITE ? 1 : -1;
                Value score = (((comp.mg * comp.phase) + (comp.eg * (256 - comp.phase))) * sign) / 256 + eval::tempo;

                double sig = sigmoid(score);
                double error = sig - all.result(idx);
                ploss += error * error;

                // Avoid division by zero in gradient
                if (std::abs(error) < 1e-12)
                    continue;

                double sig_deriv = 0.004 * sig * (1.0 - sig);
                double common_factor = 2.0 * error * sig_deriv;
                double phase_mg = comp.phase / 256.0;
                double phase_eg = (256 - comp.phase) / 256.0;
                double stm_sign_val = (pos.side_to_move() == chess::WHITE) ? 1.0 : -1.0;

                accumulate_gradient(pos, common_factor, addr_map, phase_mg, phase_eg, stm_sign_val, pg);
            }
            partial_loss[tid] = ploss;
        };

        for (int t = 0; t < num_threads; t++) {
            size_t start = t * chunk;
            size_t end = std::min(start + chunk, n);
            if (start >= n)
                break;
            threads.emplace_back(worker, t, start, end);
        }
        for (auto &t : threads)
            t.join();

        double loss_sum = std::accumulate(partial_loss.begin(), partial_loss.end(), 0.0);
        double train_loss = loss_sum / n;

        // Accumulate gradients from all threads
        for (int i = 0; i < dim; i++) {
            gradient[i] = 0.0;
            for (int t = 0; t < num_threads; t++) {
                gradient[i] += partial_grad[t][i];
            }
        }

        std::vector<double> old_x(dim);
        for (int i = 0; i < dim; i++)
            old_x[i] = *params[i].value_ptr;

        // Gradient clipping to prevent exploding gradients
        double max_abs_grad = 0.0;
        for (int i = 0; i < dim; i++) {
            // Clip gradient magnitude
            if (std::abs(gradient[i]) > 1000.0) {
                gradient[i] = (gradient[i] > 0) ? 1000.0 : -1000.0;
            }
            max_abs_grad = std::max(max_abs_grad, std::abs(gradient[i]));
        }
        if (max_abs_grad < 1e-12)
            max_abs_grad = 1.0;

        double step_size = lr;
        double new_loss = train_loss;
        int halvings = 0;

        for (int attempt = 0; attempt < 12; attempt++) {
            double scale = step_size / max_abs_grad;
            bool any_change = false;

            // Store current parameter values before making changes
            std::vector<int> current_values(dim);
            for (int i = 0; i < dim; i++) {
                current_values[i] = *params[i].value_ptr;
            }

            // Apply gradient step
            for (int i = 0; i < dim; i++) {
                double nv = old_x[i] - scale * gradient[i];
                nv = std::clamp(nv, (double)params[i].lower, (double)params[i].upper);
                int vi = int(std::round(nv));
                if (vi != *params[i].value_ptr)
                    any_change = true;
                *params[i].value_ptr = vi;
            }

            if (!any_change) {
                // No parameter changed, restore and break
                for (int i = 0; i < dim; i++) {
                    *params[i].value_ptr = current_values[i];
                }
                break;
            }

            // Evaluate new loss
            new_loss = eval_loss(all, train_idx);

            if (new_loss < train_loss) {
                // Improvement found, keep changes and update learning rate
                lr = step_size;
                break;
            } else {
                // No improvement, restore parameters and try smaller step
                for (int i = 0; i < dim; i++) {
                    *params[i].value_ptr = current_values[i];
                }
                step_size *= 0.5;
                halvings++;
            }
        }

        double hold_loss = eval_loss(all, holdout_idx);

        if (hold_loss < best_loss) {
            best_loss = hold_loss;
            for (int i = 0; i < dim; i++)
                best_x[i] = *params[i].value_ptr;
        }

        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() / 1000.0;

        std::cout << "info string iter=" << iter << " train=" << train_loss << " hold=" << hold_loss << " lr=" << lr
                  << " halvings=" << halvings << " max_grad=" << max_abs_grad << " time=" << elapsed << "s" << std::endl;

        if (new_loss >= best_loss && halvings > 0 && step_size < 1.0 / 128.0) {
            std::cout << "info string no further improvement, stopping" << std::endl;
            break;
        }
    }

    for (int i = 0; i < dim; i++)
        *params[i].value_ptr = int(std::round(best_x[i]));

    // Export the best weights found
    std::fstream file(weights_header, std::ios::out);
    if (file.is_open())
        Tune::export_weights(file);
    else
        std::cerr << "info string failed to open " << weights_header << " for writing\n";
}

} // namespace

void tune_command(const std::string &csv_path, int iterations, int max_pos, const std::string &weights_header) {
    if (max_pos <= 0) {
        std::cerr << "info string max_pos must be positive\n";
        return;
    }
    if (iterations <= 0) {
        std::cerr << "info string iterations must be positive\n";
        return;
    }
    auto start_time = std::chrono::steady_clock::now();

    csv::CSVReader reader(csv_path);
    TuneData all;
    std::random_device rd;
    std::mt19937_64 load_rng(rd());
    size_t seen = 0;
    for (auto &row : reader) {
        auto fen = row["fen"].get<>();
        auto result = row["result"].get<double>();
        seen++;
        if (all.size() < (size_t)max_pos) {
            all.add(fen, result);
        } else {
            std::uniform_int_distribution<size_t> dist(0, seen - 1);
            size_t j = dist(load_rng);
            if (j < all.size()) {
                all.offsets[j] = all.fen_buf.size();
                all.fen_buf.insert(all.fen_buf.end(), fen.begin(), fen.end());
                all.fen_buf.push_back('\0');
                all.results[j] = result;
            }
        }
    }
    if (all.size() == 0) {
        std::cerr << "info string no positions loaded from CSV\n";
        return;
    }
    std::cout << "info string loaded " << all.size() << " positions via reservoir sampling (seen " << seen << ")" << std::endl;

    size_t n = all.size();
    size_t holdout_size = std::min(size_t(2000), n / 5);

    std::vector<size_t> idx(n);
    for (size_t i = 0; i < n; i++)
        idx[i] = i;
    std::mt19937 rng(rd());
    std::shuffle(idx.begin(), idx.end(), rng);

    std::vector<size_t> holdout_idx(idx.begin(), idx.begin() + holdout_size);
    std::vector<size_t> train_idx(idx.begin() + holdout_size, idx.end());

    auto params = collect_params();
    int dim = (int)params.size();

    std::cout << "info string loaded " << n << " positions, dim=" << dim << " train=" << train_idx.size()
              << " holdout=" << holdout_idx.size() << std::endl;

    double base_train = eval_loss(all, train_idx);
    double base_hold = eval_loss(all, holdout_idx);
    std::cout << "info string baseline train=" << base_train << " holdout=" << base_hold << std::endl;

    texel_tune(all, train_idx, holdout_idx, params, iterations, weights_header);

    double final_hold = eval_loss(all, holdout_idx);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();

    std::cout << "info string tune done in " << elapsed << "s, best holdout=" << final_hold * holdout_size
              << " (baseline=" << base_hold * holdout_size << ")" << std::endl;
    std::fstream file(weights_header, std::ios::out);
    if (file.is_open())
        Tune::export_weights(file);
    else
        std::cerr << "info string failed to open " << weights_header << " for writing\n";
}

} // namespace engine
