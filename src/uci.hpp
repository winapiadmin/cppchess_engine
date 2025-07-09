#pragma once
#include "chess.hpp"
#include "search.h"
#include "ucioptions.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
void uci_loop();
void handle_bench();  // main()
int  to_cp(Stockfish::Value v, const Stockfish::Position& pos);
