#include "uci.h"
#include "eval.h"
#include "search.h"
#include "timeman.h"
#include "tune.h"
#ifdef USE_CSV_PARSER
#include "tune_cmd.h"
#endif
#include "ucioption.h"
#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <iostream>
#include <position.h>
#include <printers.h>
#include <sstream>
#include <thread>
using namespace engine;
bool quit{ false };
chess::Position pos;
OptionsMap engine::options;
std::thread searchThread;

namespace {
std::string strip_optional_quotes(std::string fen) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };

    fen.erase(fen.begin(), std::find_if(fen.begin(), fen.end(), notSpace));
    fen.erase(std::find_if(fen.rbegin(), fen.rend(), notSpace).base(), fen.end());

    if (fen.size() >= 2 && fen.front() == '"' && fen.back() == '"')
        return fen.substr(1, fen.size() - 2);

    if (!fen.empty() && fen.front() == '"')
        fen.erase(fen.begin());

    if (!fen.empty() && fen.back() == '"')
        fen.pop_back();

    return fen;
}
} // namespace

void engine::stop() {
    search::stop();
    if (searchThread.joinable()) {
        searchThread.join();
    }
}
void handlePosition(std::istringstream &is) {
    stop();
    std::string token, fen;

    is >> token;

    if (token == "startpos") {
        fen = chess::Position::START_FEN;
        is >> token; // Consume the "moves" token, if any
    } else if (token == "fen")
        while (is >> token && token != "moves")
            fen += token + " ";
    else
        return;

    try {
        pos.setFEN(strip_optional_quotes(fen));
    } catch (const std::exception &e) {
        std::cout << "info string Invalid FEN: " << e.what() << std::endl;
        return;
    }

    while (is >> token) {
        try {
            pos.push_uci(token);
        } catch (const std::exception &e) {
            std::cout << "info string Invalid move " << token << ": " << e.what() << std::endl;
            return;
        }
    }
}
timeman::LimitsType parse_limits(std::istream &is) {
    timeman::LimitsType limits;
    std::string token;

    while (is >> token)
        if (token == "searchmoves") // Needs to be the last command on the line
            while (is >> token) {
                std::transform(token.begin(), token.end(), token.begin(), [](auto c) { return std::tolower(c); });
                limits.searchmoves.push_back(token);
            }
        else if (token == "wtime")
            is >> limits.time[chess::WHITE];
        else if (token == "btime")
            is >> limits.time[chess::BLACK];
        else if (token == "winc")
            is >> limits.inc[chess::WHITE];
        else if (token == "binc")
            is >> limits.inc[chess::BLACK];
        else if (token == "movestogo")
            is >> limits.movestogo;
        else if (token == "depth")
            is >> limits.depth;
        else if (token == "nodes")
            is >> limits.nodes;
        else if (token == "movetime")
            is >> limits.movetime;
        else if (token == "mate")
            is >> limits.mate;
        else if (token == "perft")
            is >> limits.perft;
        else if (token == "infinite")
            limits.infinite = 1;
        else if (token == "ponder") {
            limits.ponderMode = true;
        }

    return limits;
}

void handleGo(std::istringstream &ss) {
    stop();
    if (searchThread.joinable())
        searchThread.join();
    chess::Position copy = pos;

    searchThread = std::thread([copy, ss = std::move(ss)]() mutable { search::search(copy, parse_limits(ss)); });
}
template <typename... Ts> struct overload : Ts... {
    using Ts::operator()...;
};
template <typename... Ts> overload(Ts...) -> overload<Ts...>;

std::string engine::format_score(const Score &s) {
    const auto format =
        overload{ [](Score::Mate mate) -> std::string {
                     auto m = mate.plies > 0 ? (mate.plies + 1) : (mate.plies - 1) / 2;
                     return std::string("mate ") + std::to_string(m);
                 },
                  [](Score::Tablebase tb) -> std::string {
                      constexpr int TB_CP = 20000;
                      return std::string("cp ") + std::to_string((tb.win ? TB_CP - tb.plies : -TB_CP - tb.plies));
                  },
                  [](Score::InternalUnits units) -> std::string { return std::string("cp ") + std::to_string(units.value); } };

    return s.visit(format);
}

void engine::report(const InfoShort &info) {
    std::cout << "info depth " << info.depth << " score " << format_score(info.score) << std::endl;
}

void engine::report(const InfoFull &info, bool showWDL) {
    std::stringstream ss;

    ss << "info";
    ss << " depth " << info.depth                //
       << " seldepth " << info.selDepth          //
       << " multipv " << info.multiPV            //
       << " score " << format_score(info.score); //

    if (!info.bound.empty())
        ss << " " << info.bound;

    if (showWDL)
        ss << " wdl " << info.wdl;

    ss << " nodes " << info.nodes       //
       << " nps " << info.nps           //
       << " hashfull " << info.hashfull //
       << " tbhits " << info.tbHits     //
       << " time " << info.timeMs       //
       << " pv " << info.pv;            //
    if (!info.extrainfo.empty())
        ss << "\ninfo string extras " << info.extrainfo;
    std::cout << ss.str() << std::endl;
}

void engine::report(const InfoIteration &info) {
    std::stringstream ss;

    ss << "info";
    ss << " depth " << info.depth                    //
       << " currmove " << info.currmove              //
       << " currmovenumber " << info.currmovenumber; //

    std::cout << ss.str() << std::endl;
}

void engine::report(std::string_view bestmove) {
    std::cout << "bestmove " << bestmove;
    std::cout << std::endl;
}
void execCmd(const std::string &line) {
    std::istringstream ss(line);
    std::string token;

    while (ss >> token) {
        if (token == "uci") {
            std::cout << "id name cppchess_engine\n";
            std::cout << "id author winapiadmin\n";
            std::cout << options << '\n';
            std::cout << "uciok\n";
            break;
        } else if (token == "isready") {
            std::cout << "readyok\n";
            break;
        } else if (token == "position") {
            stop();
            handlePosition(ss);
            break;
        } else if (token == "go") {
            stop();
            handleGo(ss);
            break; // rest belongs to go
        } else if (token == "ucinewgame") {
            stop();
            search::tt.clear();
            break;
        } else if (token == "stop") {
            stop();
            break;
        } else if (token == "quit") {
            stop();
            quit = true;
            return;
        } else if (token == "setoption") {
            stop();
            options.setoption(ss);
            break;
        } else if (token == "visualize" || token == "d") {
            std::cout << pos << std::endl;
            break;
        } else if (token == "eval") {
            handlePosition(ss); // auto-handle it if any
            int score = eval::eval(pos);
            if (pos.side_to_move() != chess::WHITE)
                score = -score;
            std::cout << score << std::endl;
            break;
        } else if (token == "bench") {
            uint64_t nodes = 0;
            const std::vector<std::string> Defaults = {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
                "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11",
                "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
                "rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14 moves d4e6",
                "r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14 moves g2g4",
                "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
                "r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
                "r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
                "4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
                "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
                "r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
                "3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
                "r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
                "4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
                "3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
                "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
                "3b4/5kp1/1p1p1p1p/pP1PpP1P/P1P1P3/3KN3/8/8 w - - 0 1",
                "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1 moves g5g6 f3e3 g6g5 e3f3",
                "8/6pk/1p6/8/PP3p1p/5P2/4KP1q/3Q4 w - - 0 1",
                "7k/3p2pp/4q3/8/4Q3/5Kp1/P6b/8 w - - 0 1",
                "8/2p5/8/2kPKp1p/2p4P/2P5/3P4/8 w - - 0 1",
                "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
                "8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - 0 1",
                "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
                "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
                "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
                "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
                "6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - 0 1",
                "8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 1",
                "5rk1/q6p/2p3bR/1pPp1rP1/1P1Pp3/P3B1Q1/1K3P2/R7 w - - 93 90",
                "4rrk1/1p1nq3/p7/2p1P1pp/3P2bp/3Q1Bn1/PPPB4/1K2R1NR w - - 40 21",
                "r3k2r/3nnpbp/q2pp1p1/p7/Pp1PPPP1/4BNN1/1P5P/R2Q1RK1 w kq - 0 16",
                "3Qb1k1/1r2ppb1/pN1n2q1/Pp1Pp1Pr/4P2p/4BP2/4B1R1/1R5K b - - 11 40",
                "4k3/3q1r2/1N2r1b1/3ppN2/2nPP3/1B1R2n1/2R1Q3/3K4 w - - 5 1",
                "1r6/1P4bk/3qr1p1/N6p/3pp2P/6R1/3Q1PP1/1R4K1 w - - 1 42",

                // 5-man positions
                "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",  // Kc2 - mate
                "8/8/8/5N2/8/p7/8/2NK3k w - - 0 1",   // Na2 - mate
                "8/3k4/8/8/8/4B3/4KB2/2B5 w - - 0 1", // draw

                // 6-man positions
                "8/8/1P6/5pr1/8/4R3/7k/2K5 w - - 0 1",  // Re5 - mate
                "8/2p4P/8/kr6/6R1/8/8/1K6 w - - 0 1",   // Ka2 - mate
                "8/8/3P3k/8/1p6/8/1P6/1K3n2 b - - 0 1", // Nd2 - draw

                // 7-man positions
                "8/R7/2q5/8/6k1/8/1P5p/K6R w - - 0 124", // Draw

                // Mate and stalemate positions
                "6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1",
                "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1",
                "8/8/8/8/8/6k1/6p1/6K1 w - -",
                "7k/7P/6K1/8/3B4/8/8/8 b - -",
            };
            timeman::LimitsType tc{};tc.depth=10;
            auto start = std::chrono::high_resolution_clock::now();
            for (auto& fen: Defaults){
                std::cerr<<fen<<'\n';
                chess::Position pos(fen);
                nodes+=search::search(pos, tc);
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            std::cout << "nodes " << nodes << " nps " << nodes / (duration_ms / 1000.0) << std::endl;
        } else if (token == "export_weights") {
            std::string weights_header = "Weights.h";
            ss >> weights_header;
            std::fstream file(weights_header, std::ios::out);
            if (file.is_open()) {
                Tune::export_weights(file);
                std::cout << "info string dumped weights to " << weights_header << '\n';
            } else {
                std::cout << "info string failed to open " << weights_header << " for writing\n";
            }
            break;
        } else if (token == "tune") {
            stop();
#ifdef USE_CSV_PARSER
            std::string csv_path, out_file = "Weights.h";
            int iters = 50, max_pos = 20000;
            ss >> csv_path >> iters >> max_pos;
            tune_command(csv_path, iters, max_pos, out_file);
#else
            std::cout << "info string engine built without tuning support" << std::endl;
#endif
            break;
        } else if (token == "evalbatch") {
            char c;
            bool first = true;
            while (ss >> c) {
                if (c != '"')
                    continue;
                std::string fen;
                while (ss.get(c) && c != '"')
                    fen += c;
                if (!first)
                    std::cout << ' ';
                first = false;
                try {
                    chess::Position bp;
                    bp.setFEN(fen);
                    int sc = eval::eval(bp);
                    if (bp.side_to_move() != chess::WHITE)
                        sc = -sc;
                    std::cout << sc;
                } catch (...) {
                    std::cout << "0";
                }
            }
            std::cout << std::endl;
            break;
        }
    }
}
void engine::loop() {
    std::string line;
    pos.setFEN(chess::Position::START_FEN);

    while (!quit && std::getline(std::cin, line)) {
        execCmd(line);
    }
    stop();
}
