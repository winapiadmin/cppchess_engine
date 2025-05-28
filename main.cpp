// File: main.cpp
#include "chess.hpp"
#include "eval.h"
#include "search.hpp"
#include "tt.hpp"
#include "ucioptions.hpp" // this only handles options
#include "uci.h"

using namespace std;


// Main UCI loop
int main()
{
    chess::Position board;
    UCIOptions::addSpin("aspiration_window", 30, 0, 500);
    uci_loop();
    return 0;
}
