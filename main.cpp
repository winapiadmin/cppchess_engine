// File: main.cpp
#include "chess.hpp"
#include "eval.h"
#include "search.hpp"
#include "tt.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>

using namespace std;

int main()
{
    chess::Position board;
    string line;

    cout << "id name MyEngine" << endl;
    cout << "id author YourNameHere" << endl;
    cout << "uciok" << endl;

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
                fen = "";
                int fenParts = 6;
                while (fenParts-- && iss >> temp)
                {
                    fen += temp + " ";
                }
                board.setFen(fen);
                if (iss >> sub && sub == "moves")
                {
                    while (iss >> sub)
                    {
                        board.makeMove(chess::uci::uciToMove(board, sub));
                    }
                }
            }
        }
        else if (token == "go")
        {
            int depth = 6;
            string sub;
            while (iss >> sub)
            {
                if (sub == "depth")
                    iss >> depth;
            }
            if (depth < 1)
                depth = 1;
            chess::Move best,ponder;
            for (int i = 1; i <= depth; i++)
            {
                clock_t t1 = clock();

                search::tt.newSearch();
                int16_t score = search::alphaBeta(board, -MATE(0), MATE(0), i);

                clock_t t2 = clock();
                double seconds = double(t2 - t1) / CLOCKS_PER_SEC;
                best = search::pv.argmove[0];
                ponder = search::pv.argmove[1];
                fprintf(stderr, "info depth %d score cp %d nodes %llu time %d nps %.0f\n",
                        i, score, search::nodes, (t2-t1)*1000/CLOCKS_PER_SEC, search::nodes / seconds);

            }
            cout << "bestmove " << best;
            if (ponder != chess::Move())
                cout << " ponder " << ponder;
            cout << "\n";
        }
        else if (token == "d"){
            cout << board << endl;
        }
        else if (token == "quit")
        {
            break;
        }
    }

    return 0;
}
