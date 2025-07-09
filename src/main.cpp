#include <cstring>
#include "search.h"
#include "uci.hpp"
#include "ucioptions.hpp"

int main(int argc, char** argv) {
    UCIOptions::addSpin("Hash", 16, 1, 1048576, [](const UCIOptions::Option& opt) {
        search::tt.resize(std::get<int>(opt.value));
    });
    UCIOptions::setOption("Hash", "16");
    search::init<true>();
    UCIOptions::addString("NNUEEvalFileBig", EvalFileDefaultNameBig,
                          [](const UCIOptions::Option& opt) {
                              search::nn->big.load(".", std::get<std::string>(opt.value));
                          });
    UCIOptions::addString("NNUEEvalFileSmall", EvalFileDefaultNameSmall,
                          [](const UCIOptions::Option& opt) {
                              search::nn->small.load(".", std::get<std::string>(opt.value));
                          });
    search::init<false>();
    if (argc == 2 && !strcmp(argv[1], "bench"))  // SF bench (Makefile)
    {
        handle_bench();
        return 0;
    }
    uci_loop();
    return 0;
}
