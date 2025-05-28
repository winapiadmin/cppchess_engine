#include "uci.h"

chess::Position board;
// Thread handle
static std::thread search_thread;
constexpr unsigned INFINITE_TIME = std::numeric_limits<unsigned>::max();
void handleSetOption(const std::string& input) {
    std::istringstream iss(input);
    std::string token, name, value;
    bool reading_name = false, reading_value = false;

    while (iss >> token) {
        if (token == "setoption") continue;
        if (token == "name") {
            reading_name = true;
            reading_value = false;
            name.clear();
            value.clear();
            continue;
        }
        if (token == "value") {
            reading_value = true;
            reading_name = false;
            continue;
        }

        if (reading_name) {
            if (!name.empty()) name += " ";
            name += token;
        } else if (reading_value) {
            if (!value.empty()) value += " ";
            value += token;
        }
    }

    // Clean up name
    name.erase(name.find_last_not_of(" \n\r\t") + 1);

    if (!UCIOptions::options.count(name)) {
        std::cerr << "info string Unknown option: " << name << "\n";
        return;
    }

    auto& opt = UCIOptions::options[name];

    if (opt.type == UCIOptions::Option::BUTTON) {
        // Optional: callback or manual hook
        std::cout << "info string Button pressed: " << name << "\n";
        return;
    }

    if (value.empty()) {
        std::cerr << "info string No value provided for option: " << name << "\n";
        return;
    }

    UCIOptions::setOption(name, value);
}
void handle_uci() {
    std::cout << "id name MyEngine\n";
    std::cout << "id author YourNameHere\n";
    UCIOptions::printOptions();
    std::cout << "uciok\n";
}

inline void handle_isready() { std::cout << "readyok\n"; }

void handle_quit() {
    search::stop_requested = true;
    if (search_thread.joinable()) search_thread.join();
    std::exit(0);
}

inline void handle_stop() { search::stop_requested = true; }

inline void handle_display() { std::cout << board << std::endl; }

inline void handle_eval() { trace(board); }

void handle_position(std::istringstream& iss) {
    std::string sub;
    iss >> sub;

    if (sub == "startpos") {
        board.setFen(chess::constants::STARTPOS);
        if (iss >> sub && sub == "moves") {
            while (iss >> sub) board.makeMove(chess::uci::uciToMove(board, sub));
        }
    } else if (sub == "fen") {
        std::string fen, temp;
        int fenParts = 6;
        while (fenParts-- && iss >> temp) fen += temp + " ";
        board.setFen(fen);
        if (iss >> sub && sub == "moves") {
            while (iss >> sub) board.makeMove(chess::uci::uciToMove(board, sub));
        }
    }

    search::tt.clear();
}

void handle_bench() {
    chess::Position board;
    uint64_t nodes = 0;
    std::vector<std::string> fen_list = {"r7/pp3kb1/7p/2nr4/4p3/7P/PP1N1PP1/R1B1K2R b KQ - 1 19",
                                         "r1bqk2r/pppp1ppp/2n5/4p3/4P3/2N5/PPPP1PPP/R1BQK2R w KQkq - 0 1",
                                         "rnbqkb1r/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                                         "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1",
                                         "8/pp3k2/7p/2nR4/4p3/7P/Pb3PP1/6K1 b - - 1 25",
                                         "8/p4k2/1p1R3p/2n5/4p3/7P/Pb3PP1/6K1 b - - 1 26",
                                         "8/p4k2/1p1R1b1p/2n5/4p3/7P/P4PP1/5K2 b - - 3 27",
                                         "8/p3k3/1pR2b1p/2n5/4p3/7P/P4PP1/5K2 b - - 5 28",
                                         "1R6/8/1p1knb1p/p7/4p3/7P/P3KPP1/8 b - - 3 32",
                                         "3R4/8/1p1k3p/p7/3bp2P/6Pn/P3KP2/8 b - - 2 35",
                                         "8/4R3/1p6/p5PP/3b4/2n2K2/P2kp3/8 w - - 1 46",
                                         "rnbqkb1r/ppp1pppp/1n6/8/8/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 2 5",
                                         "r2qr1k1/1ppb1pbp/np4p1/3Pp3/4N3/P2B1N1P/1PP2PP1/2RQR1K1 b - - 6 16"};
    auto start_time = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < fen_list.size(); ++i) {
        std::cout << "Position " << i << "/" << fen_list.size() - 1 << std::endl;
        board.setFen(fen_list[i]);
        search::run_search(board, 8, INFINITE_TIME, false);
        nodes += search::nodes;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "===========================\n";
    std::cout << "Total time (ms) : " << static_cast<int>(elapsed.count() * 1000) << "\n";
    std::cout << "Nodes searched  : " << nodes << "\n";
    std::cout << "Nodes/second    : " << static_cast<uint64_t>(nodes / elapsed.count()) << "\n";
}
void handle_go(std::istringstream& iss) {
    int depth = -1;
    int movetime = -1;
    bool infinite = false;
    bool white = (board.sideToMove() == chess::Color::WHITE);
    std::string sub;

    while (iss >> sub) {
        if (sub == "depth") iss >> depth;
        else if (sub == "movetime") iss >> movetime;
        else if (sub == "wtime" && white) iss >> movetime;
        else if (sub == "btime" && !white) iss >> movetime;
        else if (sub == "infinite") {
            infinite = true;
            depth = MAX_PLY;
        }
    }

    if (search_thread.joinable()) {
        search::stop_requested = true;
        search_thread.join();
    }

    search::stop_requested = false;
    unsigned time_limit_ms = infinite ? INFINITE_TIME : (movetime >= 0 ? movetime : INFINITE_TIME);
    chess::Position board_copy = board;

    search_thread = std::thread(search::run_search, board_copy, depth, time_limit_ms, infinite);
    search_thread.detach();
}
void handle_ucinewgame() {
    board.setFen(chess::constants::STARTPOS);
    search::tt.clear();
    search::tt.newSearch();
}
void uci_loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        if (token == "ucinewgame") handle_ucinewgame();
        else if (token == "uci") handle_uci();
        else if (token == "isready") handle_isready();
        else if (token == "quit") handle_quit();
        else if (token == "stop") handle_stop();
        else if (token == "setoption") handleSetOption(line);
        else if (token == "position") handle_position(iss);
        else if (token == "go") handle_go(iss);
        else if (token == "d") handle_display();
        else if (token == "bench") handle_bench();
        else if (token == "eval") handle_eval();
    }
}
