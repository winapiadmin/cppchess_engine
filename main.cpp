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
#define MAX_SCORE_CP 31000
#define MATE(ply) (32000 - (ply))
#define IS_MATE_SCORE(score) (abs(score) >= MAX_SCORE_CP)
#define MATE_DISTANCE(score) (MATE(0) - abs(score))
constexpr unsigned INFINITE_TIME = std::numeric_limits<unsigned>::max();
constexpr unsigned DEFAULT_TIME_LIMIT = 5000;

// Thread handle
static std::thread search_thread;

// Search function
void run_search(chess::Position board, int depth, unsigned time_limit_ms, bool infinite)
{
    int prev_score = 0;

    search::tt.newSearch();
    search::start_time = std::chrono::high_resolution_clock::now();
    search::time_limit = std::chrono::milliseconds((int)(time_limit_ms*0.2));
    search::PV stablePV;

    clock_t t1 = clock();

    for (int d = 1; (depth == -1 || d <= depth) && !search::stop_requested.load(); ++d)
    {
        int alpha = prev_score - ASPIRATION_DELTA;
        int beta = prev_score + ASPIRATION_DELTA;

        int16_t score = search::alphaBeta(board, alpha, beta, d);
        memcpy(&stablePV, &search::pv, sizeof(search::PV));

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

        if (score > prev_score)
        {
            memcpy(&stablePV, &search::pv, sizeof(search::PV));
        }

        prev_score = score;

        clock_t t2 = clock();
        double elapsed_ms = double(t2 - t1) / CLOCKS_PER_SEC * 1000.0;

        cout << "info depth " << d
             << " score";
        if (!IS_MATE_SCORE(score))
            cout << " cp " << score;
        else
            cout << " mate " << (score < 0 ? "-" : "") << ((MATE_DISTANCE(score) + 1) / 2);
        cout << " time " << int(elapsed_ms)
             << " nodes " << search::nodes
             << " nps " << int(search::nodes / max(elapsed_ms / 1000.0, 1e-3))
             << " pv ";

        for (int i = 0; i < 256 && stablePV.argmove[i] != chess::Move(); i++)
            cout << stablePV.argmove[i] << " ";
        cout << endl;

        if (!infinite && search::check_time())
            break;
    }

    chess::Move best = stablePV.argmove[0];
    chess::Move ponder = stablePV.argmove[1];

    if (best != chess::Move())
    {
        cout << "bestmove " << best;
        if (ponder != chess::Move())
            cout << " ponder " << ponder;
        cout << endl;
    }
    else
    {
        cout << "bestmove 0000" << endl;
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
            if (search_thread.joinable())
                search_thread.join();
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
            unsigned time_limit_ms = infinite ? INFINITE_TIME : (movetime >= 0 ? movetime : DEFAULT_TIME_LIMIT);
            chess::Position board_copy = board;

            search_thread = std::thread(run_search, board_copy, depth, time_limit_ms, infinite);
            //search_thread.detach();
        }
        else if (token == "d")
        {
            cout << board << endl;
        }
        else if (token == "bench")
        {
            board.setFen(chess::constants::STARTPOS);
            cout << "Startpos Eval: " << eval(board) << endl;

            board.setFen("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPP1/RNBQKBNR w KQkq - 0 1");
            cout << "White up a queen: " << eval(board) << endl;

            board.setFen("7k/8/8/8/8/8/8/7R b - - 0 1");
            cout << "Mate Eval: " << eval(board) << endl;
        }
    }

    return 0;
}
