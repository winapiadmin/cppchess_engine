// File: main.cpp
#include "chess.hpp"
#include "eval.h"
#include "search.hpp"
#include "tt.hpp"
#include <atomic>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
using namespace std;

int calculateTimeLimit(int movetime, int wtime, int btime, int winc, int binc, bool whiteToMove, int movesToGo) {
    int time_limit_ms = -1;

    // If movetime is specified, use it directly
    if (movetime > 0) {
        time_limit_ms = movetime;  // Time limit in milliseconds
    }
    // If no movetime, calculate time limit based on available time and increments
    else if (wtime >= 0 && btime >= 0) {
        int my_time = whiteToMove ? wtime : btime;
        int my_inc = whiteToMove ? winc : binc;

        if (movesToGo > 0) {
            // Calculate time per move based on moves remaining
            time_limit_ms = my_time / movesToGo + my_inc / 2;
        } else {
            // If movesToGo is not provided, allocate 1/20th of the remaining time + half of the increment
            time_limit_ms = my_time / 20 + my_inc / 2;
        }

        // Ensure time limit doesn't go below a minimum of 50ms
        if (time_limit_ms < 50) time_limit_ms = 50;
    }

    return time_limit_ms;
}

int main() {
    chess::Position board;
    string line;

    while (getline(cin, line)) {
        istringstream iss(line);
        string token;
        iss >> token;

        if (token == "stop") {
            search::stop_requested = true;
            continue; // Let "go" loop check it
        } else if (token == "go") {
            int depth = -1; // Use -1 to indicate infinite unless set
            int movetime = -1;
            int wtime = -1, btime = -1, winc = 0, binc = 0;
            bool infinite = false;
            int movesToGo = 0; // Default, or you can get this from the UCI input

            string sub;
            while (iss >> sub) {
                if (sub == "depth")
                    iss >> depth;
                else if (sub == "movetime")
                    iss >> movetime;
                else if (sub == "wtime")
                    iss >> wtime;
                else if (sub == "btime")
                    iss >> btime;
                else if (sub == "winc")
                    iss >> winc;
                else if (sub == "binc")
                    iss >> binc;
                else if (sub == "movesToGo")
                    iss >> movesToGo;
                else if (sub == "infinite") {
                    infinite = true;
                    depth = MAX_PLY; // Set depth to MAX_PLY for infinite search
                }
            }

            search::stop_requested = false; // Reset stop request flag at the start

            std::thread([&, depth, movetime, wtime, btime, winc, binc, infinite, movesToGo]() {
                unsigned time_limit_ms = -1;

                if (!infinite) {
                    // Calculate the time limit if not infinite
                    time_limit_ms = calculateTimeLimit(movetime, wtime, btime, winc, binc, board.sideToMove() == chess::Color::WHITE, movesToGo);
                }

                bool white_to_move = (board.sideToMove() == chess::Color::WHITE);
                chess::Move best,ponder;

                clock_t t1 = clock(); // Start the clock for timing

                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::milliseconds max_duration(time_limit_ms);  // Maximum time allowed for this search

                for (int d = 1; (depth == -1 || d <= depth) && !search::stop_requested; ++d) {
                    search::tt.newSearch();
                    
                    // If it's an infinite search, don't use the time limit, only depth
                    if (infinite) {
                        max_duration = std::chrono::milliseconds(0);  // No time limit for infinite search
                    }

                    // Check if time limit is almost exceeded before starting each depth search
                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    if (!infinite && elapsed >= max_duration * 0.3) {  // 30% of the total time
                        break;
                    }

                    int16_t score = search::alphaBeta(board, -MATE(0), MATE(0), d);

                    // Check if stop has been requested by the user
                    if (search::stop_requested) {
                        break;
                    }

                    // Measure elapsed time for this depth
                    clock_t t2 = clock();
                    double elapsed_ms = double(t2 - t1) / CLOCKS_PER_SEC * 1000;

                    best = search::pv.argmove[0];
                    ponder = search::pv.argmove[1];
                    std::cout << "info depth " << d
                            << " score cp " << score
                            << " time " << int(elapsed_ms)
                            << " nodes " << search::nodes
                            << " nps " << int(search::nodes / std::max(elapsed_ms / 1000.0, 1e-3))
                            << " pv ";
                    search::printPV(board);

                    // Check if we've reached or exceeded the time limit (for non-infinite searches)
                    if (!infinite && elapsed_ms >= time_limit_ms) {
                        break;
                    }
                }
                // Output the best move found after the search is finished
                if (best!=chess::Move::NULL_MOVE)std::cout << "bestmove " << best;
                else std::cout<<"bestmove (none)"<<std::endl;
                if (ponder!=chess::Move::NULL_MOVE)std::cout << " ponder "<<ponder<<std::endl;
            }).detach(); // Detach the search thread to allow concurrent operation
        }

        else if (token == "uci") {
            cout << "id name MyEngine" << endl;
            cout << "id author YourNameHere" << endl;
            cout << "uciok" << endl;
        } else if (token == "isready") {
            cout << "readyok" << endl;
        } else if (token == "position") {
            string sub;
            iss >> sub;

            if (sub == "startpos") {
                board.setFen(chess::constants::STARTPOS);
                if (iss >> sub && sub == "moves") {
                    while (iss >> sub) {
                        board.makeMove(chess::uci::uciToMove(board, sub));
                    }
                }
            } else if (sub == "fen") {
                string fen, temp;
                fen = "";
                int fenParts = 6;
                while (fenParts-- && iss >> temp) {
                    fen += temp + " ";
                }
                board.setFen(fen);
                if (iss >> sub && sub == "moves") {
                    while (iss >> sub) {
                        board.makeMove(chess::uci::uciToMove(board, sub));
                    }
                }
            }
            search::tt.clear();
        } else if (token == "d") {
            cout << board << endl;
        } else if (token == "bench") {
            // Test 1: Initial position
            board.setFen(chess::constants::STARTPOS);
            std::cout << "Startpos Eval: " << eval(board) << std::endl;

            // Test 2: White up a queen
            board.setFen("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPP1/RNBQKBNR w KQkq - 0 1");
            std::cout << "White up a queen: " << eval(board) << std::endl;

            // Test 3: Black checkmates white
            board.setFen("7k/8/8/8/8/8/8/7R b - - 0 1");
            std::cout << "Mate Eval: " << eval(board) << std::endl;

        } else if (token == "quit") {
            break;
        }
    }

    return 0;
}
