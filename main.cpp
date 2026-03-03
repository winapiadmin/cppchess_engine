#include "ucioption.h"
#include "uci.h"
#include "search.h"
#include <iostream>
using namespace engine;
int main() {
	options.add("Move Overhead", Option(10, 0, 1000));
    options.add(  //
        "Hash", Option(16, 1, 1<<25, [](const Option &o) {
			search::tt.resize((o.operator int()));
            return std::nullopt;
            }));

    options.add(  //
        "Clear Hash", Option(+[](const Option &) {
            search::tt.clear();
            return std::nullopt;
            }));
    loop();
}