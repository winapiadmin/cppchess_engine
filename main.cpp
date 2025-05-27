// File: main.cpp
#include "chess.hpp"
#include "eval.h"
#include "search.hpp"
#include "tt.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>

using namespace std;

// Constants
#define IS_MATE_SCORE(score) (abs(score) >= MAX_SCORE_CP)
constexpr unsigned INFINITE_TIME = std::numeric_limits<unsigned>::max();

// Thread handle
static std::thread search_thread;

// Search function
void run_search(chess::Position board, int depth, unsigned time_limit_ms, bool infinite)
{
    int prev_score = 0;
    search::nodes=0;
    search::tt.newSearch();
    search::start_time = std::chrono::high_resolution_clock::now();
    search::time_limit = std::chrono::milliseconds((int)(time_limit_ms * 0.01));
    search::PV stablePV;

    for (int d = 1; (depth == -1 || d <= depth) && !search::stop_requested.load(); ++d)
    {
        int alpha = prev_score - ASPIRATION_DELTA;
        int beta = prev_score + ASPIRATION_DELTA;

        int16_t score = search::alphaBeta(board, alpha, beta, d);

        if (score <= alpha)
        {
            alpha = -MATE(0);
            beta = prev_score + ASPIRATION_DELTA;
            score = search::alphaBeta(board, alpha, beta, d);
        }
        else if (score >= beta)
        {
            alpha = prev_score - ASPIRATION_DELTA;
            beta = MATE(0);
            score = search::alphaBeta(board, alpha, beta, d);
        }
        if (!infinite && search::check_time())
            break;
    }

    chess::Move best = search::pv.argmove[0];
    chess::Move ponder = search::pv.argmove[1];

    if (best != chess::Move())
    {
        cout << "bestmove " << best;
        if (ponder != chess::Move())
            cout << " ponder " << ponder;
        cout << endl;
    }
    else
    {
        cout << "bestmove (none)" << endl;
    }
}

// Main UCI loop
int main()
{
    chess::Position board;
    string line;

    while (getline(cin, line))
    {
        istringstream iss(line);
        string token;
        iss >> token;

        if (token == "uci")
        {
            cout << "id name MyEngine" << endl;
            cout << "id author YourNameHere" << endl;
            cout << "uciok" << endl;
        }
        else if (token == "isready")
        {
            cout << "readyok" << endl;
        }
        else if (token == "quit")
        {
            search::stop_requested = true;
            if (search_thread.joinable())
                search_thread.join();
            break;
        }
        else if (token == "stop")
        {
            search::stop_requested = true;
        }
        else if (token == "position")
        {
            string sub;
            iss >> sub;

            if (sub == "startpos")
            {
                board.setFen(chess::constants::STARTPOS);
                if (iss >> sub && sub == "moves")
                {
                    while (iss >> sub)
                    {
                        board.makeMove(chess::uci::uciToMove(board, sub));
                    }
                }
            }
            else if (sub == "fen")
            {
                string fen, temp;
                int fenParts = 6;
                while (fenParts-- && iss >> temp)
                    fen += temp + " ";
                board.setFen(fen);
                if (iss >> sub && sub == "moves")
                {
                    while (iss >> sub)
                        board.makeMove(chess::uci::uciToMove(board, sub));
                }
            }

            search::tt.clear();
        }
        else if (token == "go")
        {
            int depth = -1;
            int movetime = -1;
            bool infinite = false;
            bool white = (board.sideToMove() == chess::Color::WHITE);

            string sub;
            while (iss >> sub)
            {
                if (sub == "depth")
                    iss >> depth;
                else if (sub == "movetime")
                    iss >> movetime;
                else if (sub == "wtime" && white)
                    iss >> movetime;
                else if (sub == "btime" && !white)
                    iss >> movetime;
                else if (sub == "infinite")
                {
                    infinite = true;
                    depth = MAX_PLY;
                }
            }

            if (search_thread.joinable())
            {
                search::stop_requested = true;
                search_thread.join();
            }

            search::stop_requested = false;
            unsigned time_limit_ms = INFINITE_TIME;
            if (!infinite && movetime >= 0)
            {
                time_limit_ms = movetime;
            }
            chess::Position board_copy = board;

            search_thread = std::thread(run_search, board_copy, depth, time_limit_ms, infinite);
            search_thread.detach();
        }
        else if (token == "d")
        {
            cout << board << endl;
        }
        else if (token == "bench")
        {
            uint64_t nodes=0;
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
            for (int i=0;i<fen_list.size();i++)
            {
                string fen = fen_list[i];
                std::cout<<"Position "<<i<<"/"<<fen_list.size()-1<<std::endl;
                board.setFen(fen);
                run_search(board, 8, INFINITE_TIME, false);
                nodes+=search::nodes;
            }
            auto end_time = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double> elapsed_seconds = end_time - start_time;
            printf("===========================\n");
            printf("Total time (ms) : %d\n", int(elapsed_seconds.count() * 1000));
            printf("Nodes searched  : %llu\n", nodes);
            printf("Nodes/second    : %llu\n", (uint64_t)(nodes/elapsed_seconds.count()));
        }
        else if (token == "eval")
        {
            trace(board);
        }
    }

    return 0;
}
