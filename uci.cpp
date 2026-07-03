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
        std::cerr << "info string Invalid FEN: " << e.what() << std::endl;
        return;
    }

    while (is >> token) {
        try {
            pos.push_uci(token);
        } catch (const std::exception &e) {
            std::cerr << "info string Invalid move " << token << ": " << e.what() << std::endl;
            return;
        }
    }
}
timeman::LimitsType parse_limits(std::istream &is) {
    timeman::LimitsType limits;
    std::string token;

    limits.startTime = timeman::now(); // The search starts as early as possible

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
                     auto m = (mate.plies > 0 ? (mate.plies + 1) : mate.plies) / 2;
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
            handlePosition(ss);
            break;
        } else if (token == "go") {
            handleGo(ss);
            break; // rest belongs to go
        } else if (token == "ucinewgame") {
            search::tt.clear();
            break;
        } else if (token == "stop") {
            break;
        } else if (token == "quit") {
            quit = true;
            return;
        } else if (token == "setoption") {
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
        } else if (token == "export_weights") {
            std::string weights_header = "Weights.h";
            ss >> weights_header;
            std::fstream file(weights_header, std::ios::out);
            Tune::export_weights(file);
            std::cout << "Dumped weights to " << weights_header << '\n';
            break;
        } else if (token == "tune") {
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
        stop();
        execCmd(line);
    }
    stop();
    if (searchThread.joinable())
        searchThread.join();
}
