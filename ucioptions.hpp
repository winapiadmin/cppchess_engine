#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <iostream>

namespace UCIOptions {

struct Option {
    enum Type { CHECK, SPIN, COMBO, BUTTON, STRING } type;

    std::string default_str;
    std::vector<std::string> vars;
    int min = 0, max = 0;
    std::variant<int, std::string> value;

    Option() = default;
    Option(Type t, int def, int min_ = 0, int max_ = 0);
    Option(Type t, const std::string& def);
    Option(Type t, const std::string& def, const std::vector<std::string>& options);
};

// Declare the options map (accessible to other files)
extern std::unordered_map<std::string, Option> options;

void addSpin(const std::string& name, int defaultValue, int min, int max);
void addCheck(const std::string& name, bool defaultValue);
void addString(const std::string& name, const std::string& defaultValue);
void addCombo(const std::string& name, const std::string& defaultValue, const std::vector<std::string>& vars);
void addButton(const std::string& name);

void setOption(const std::string& name, const std::string& val);
int getInt(const std::string& name);
std::string getString(const std::string& name);
void printOptions();

}  // namespace UCIOptions
