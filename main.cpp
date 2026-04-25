#include "search.h"
#include "uci.h"
#include "ucioption.h"
#include <iostream>
#include <thread>
using namespace engine;
extern std::thread searchThread;
int main() {
  options.add("Move Overhead", Option(10, 0, 1000));
  options.add("Hash", Option(16, 1, 1 << 25, [](const Option &o) {
                search::tt.resize((o.operator int()));
                return std::nullopt;
              }));

  options.add("Clear Hash", Option(+[](const Option &) {
                if (searchThread.joinable()) {
                  std::cout
                      << "info string In search, do not modify hash table\n";
                  return std::nullopt;
                }
                search::tt.clear();

                return std::nullopt;
              }));
  loop();
}
