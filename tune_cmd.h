#pragma once
#include <string>
namespace engine {
void tune_command(const std::string &csv_path, int iterations, int max_pos, const std::string &weights_header);
} // namespace engine
