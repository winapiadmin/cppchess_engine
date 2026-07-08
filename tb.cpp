#include "tb.h"
#include <exception>
#include <iostream>
#include <tbprobe.h>
#include <unordered_set>

namespace engine::tb {

tbprobe::syzygy::Tablebase tablebase;

namespace {
std::size_t unique_table_count(const std::unordered_map<std::string, tbprobe::syzygy::Table *> &tables) {
    std::unordered_set<const tbprobe::syzygy::Table *> uniqueTables;

    for (const auto &[_, table] : tables)
        if (table != nullptr)
            uniqueTables.insert(table);

    return uniqueTables.size();
}
} // namespace

void init(const std::string &path) {
    tablebase.close();

    if (path.empty()) {
        std::cout << "info string Found 0 WDL and 0 DTZ tablebase files\n";
        return;
    }

    tbprobe::syzygy::initialize();
    tablebase.add_directory(path, true, true);
    std::cout << "info string Found " << wdl_count() << " WDL and " << dtz_count() << " DTZ tablebase files\n";
}

std::size_t wdl_count() { return unique_table_count(tablebase.wdl); }

std::size_t dtz_count() { return unique_table_count(tablebase.dtz); }

int probe_wdl(chess::Position &board) {
    try {
        return *tablebase.get_wdl(board, TB_ERROR);
    } catch (const std::exception &) {
        return TB_ERROR;
    }
}

int probe_dtz(chess::Position &board) {
    try {
        return *tablebase.get_dtz(board, TB_ERROR);
    } catch (const std::exception &) {
        return TB_ERROR;
    }
}
} // namespace engine::tb
