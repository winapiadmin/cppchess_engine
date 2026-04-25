#include "uci.h"
#include "search.h"
#include "timeman.h"
#include "ucioption.h"
#include <algorithm>
#include <iostream>
#include <position.h>
#include <printers.h>
#include <sstream>
#include <thread>
using namespace engine;
chess::Position pos;
OptionsMap engine::options;
std::thread searchThread;
void handlePosition(std::istringstream &is) {
  if (searchThread.joinable()) {
    std::cout << "info string In search, do not modify position\n";
    return;
  }
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
  pos.setFen(fen);

  while (is >> token) {
    pos.push_uci(token);
  }
}
timeman::LimitsType parse_limits(std::istream &is) {
  timeman::LimitsType limits;
  std::string token;

  limits.startTime = timeman::now(); // The search starts as early as possible

  while (is >> token)
    if (token == "searchmoves") // Needs to be the last command on the line
      while (is >> token) {
        std::transform(token.begin(), token.end(), token.begin(),
                       [](auto c) { return std::tolower(c); });
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
    else if (token == "ponder")
      ;
  // std::cerr << "Pondering not supported!" << std::endl;

  return limits;
}

void handleGo(std::istringstream &ss) {
  if (searchThread.joinable()) {
    search::stop();
    searchThread.join();
  }

  searchThread = std::thread([ss = std::move(ss)]() mutable {
    search::search(pos, parse_limits(ss));
  });
}
template <typename... Ts> struct overload : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> overload(Ts...) -> overload<Ts...>;

std::string engine::format_score(const Score &s) {
  const auto format = overload{
      [](Score::Mate mate) -> std::string {
        auto m = (mate.plies > 0 ? (mate.plies + 1) : mate.plies) / 2;
        return std::string("mate ") + std::to_string(m);
      },
      [](Score::Tablebase tb) -> std::string {
        constexpr int TB_CP = 20000;
        return std::string("cp ") +
               std::to_string((tb.win ? TB_CP - tb.plies : -TB_CP - tb.plies));
      },
      [](Score::InternalUnits units) -> std::string {
        return std::string("cp ") + std::to_string(units.value);
      }};

  return s.visit(format);
}

void engine::report(const InfoShort &info) {
  std::cout << "info depth " << info.depth << " score "
            << format_score(info.score) << std::endl;
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
void engine::loop() {
  std::string line;
  pos.setFen(pos.START_FEN);
  std::cout << "cppchess_engine version " << BUILD_VERSION << '\n';
  while (std::getline(std::cin, line)) {
    std::istringstream ss(line);
    std::string token;
    while (ss >> token) {
      if (token == "uci") {
        std::cout << "id name cppchess_engine\n";
        std::cout << "id author winapiadmin\n";
        std::cout << options << '\n';
        std::cout << "uciok\n";
        std::cout.flush();
        break;
      } else if (token == "isready") {
        std::cout << "readyok\n";
        std::cout.flush();
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
        search::stop();
        if (searchThread.joinable())
          searchThread.join();
        break;
      } else if (token == "quit") {
        search::stop();
        if (searchThread.joinable())
          searchThread.join();
        return;
      } else if (token == "setoption") {
        options.setoption(ss);
        break;
      } else if (token == "visualize") {
        std::cout << pos << std::endl;
        break;
      }
    }
  }
  search::stop();
  if (searchThread.joinable())
    searchThread.join();
}
