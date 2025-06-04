#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <functional>
#include <charconv>

namespace UCIOptions {

struct Option {
    enum Type { CHECK, SPIN, COMBO, BUTTON, STRING } type;

    std::string default_str;
    std::vector<std::string> vars; // for COMBO
    int min = 0, max = 0;

    std::variant<int, std::string> value;

    // Optional callback when the option is set
    std::function<void(const Option&)> on_change;

    Option() = default;
    Option(Type t, int def, int min_ = 0, int max_ = 0, std::function<void(const Option&)> cb = {});
    Option(Type t, const std::string& def, std::function<void(const Option&)> cb = {});
    Option(Type t, const std::string& def, const std::vector<std::string>& options, std::function<void(const Option&)> cb = {});
};

// Declare the global options map
extern std::unordered_map<std::string, Option> options;

// Adders with optional callback support
void addSpin(const std::string& name, int defaultValue, int min, int max, std::function<void(const Option&)> cb = {});
void addCheck(const std::string& name, bool defaultValue, std::function<void(const Option&)> cb = {});
void addString(const std::string& name, const std::string& defaultValue, std::function<void(const Option&)> cb = {});
void addCombo(const std::string& name, const std::string& defaultValue, const std::vector<std::string>& vars, std::function<void(const Option&)> cb = {});
void addButton(const std::string& name, std::function<void(const Option&)> cb = {});

void setOption(const std::string& name, const std::string& val);

int getInt(const std::string& name);
std::string getString(const std::string& name);

void printOptions();

} // namespace UCIOptions
