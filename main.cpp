#include "chess.hpp"
#include "search.hpp"
#include "ucioptions.hpp"
#include "uci.hpp"
#include <iostream>
#include <cstdio>   // for setbuf
int main() {
    // Some quirks competition programming only for flushing buffers
    setbuf(stdout, NULL);
    UCIOptions::addSpin("Hash", 16, 1, 1024, [](const UCIOptions::Option& opt)->void {
        search::tt.resize(std::get<int>(opt.value));
    });
    uci_loop();
    return 0;
}
