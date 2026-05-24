#include "search.h"
#include "tune.h"
#include "uci.h"
#include "ucioption.h"
#include <iostream>
using namespace engine;
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

int main() {
  std::cout << std::unitbuf;
  std::cout << "cppchess_engine version " << BUILD_VERSION << '\n';
  options.add("Move Overhead", Option(10, 0, 1000));
  options.add("Hash", Option(16, 1, 1 << 25, [](const Option &o) {
                search::tt.resize(int(o));
                return std::nullopt;
              }));

  options.add("Clear Hash", Option(+[](const Option &) {
                search::tt.clear();

                return std::nullopt;
              }));
  Tune::init(options);
  loop();
}
