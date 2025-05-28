#include "ucioptions.hpp"
#include <algorithm>

namespace UCIOptions {

// Define the map (was static, now extern and defined here)
std::unordered_map<std::string, Option> options;

Option::Option(Type t, int def, int min_, int max_)
    : type(t), min(min_), max(max_), value(def) {}

Option::Option(Type t, const std::string& def)
    : type(t), default_str(def), value(def) {}

Option::Option(Type t, const std::string& def, const std::vector<std::string>& options)
    : type(t), default_str(def), vars(options), value(def) {}

void addSpin(const std::string& name, int defaultValue, int min, int max) {
    options[name] = Option(Option::SPIN, defaultValue, min, max);
}

void addCheck(const std::string& name, bool defaultValue) {
    options[name] = Option(Option::CHECK, defaultValue ? 1 : 0);
}

void addString(const std::string& name, const std::string& defaultValue) {
    options[name] = Option(Option::STRING, defaultValue);
}

void addCombo(const std::string& name, const std::string& defaultValue, const std::vector<std::string>& vars) {
    options[name] = Option(Option::COMBO, defaultValue, vars);
}

void addButton(const std::string& name) {
    options[name] = Option(Option::BUTTON, "");
}

void setOption(const std::string& name, const std::string& val) {
    auto& opt = options[name];
    switch (opt.type) {
        case Option::CHECK:
            opt.value = (val == "true" || val == "1");
            break;
        case Option::SPIN:
            opt.value = std::clamp(std::stoi(val), opt.min, opt.max);
            break;
        case Option::STRING:
            opt.value = val;
            break;
        case Option::COMBO:
            if (std::find(opt.vars.begin(), opt.vars.end(), val) != opt.vars.end())
                opt.value = val;
            break;
        case Option::BUTTON:
            // Optional: trigger action
            break;
    }
}

int getInt(const std::string& name) {
    const auto& opt = options.at(name);
    if (auto pval = std::get_if<int>(&opt.value)) return *pval;
    return 0;
}

std::string getString(const std::string& name) {
    const auto& opt = options.at(name);
    if (auto pval = std::get_if<std::string>(&opt.value)) return *pval;
    if (auto pval = std::get_if<int>(&opt.value)) return *pval ? "true" : "false";
    return "";
}

void printOptions() {
    for (const auto& [name, opt] : options) {
        std::cout << "option name " << name << " type ";
        switch (opt.type) {
            case Option::CHECK:
                std::cout << "check default " << std::get<int>(opt.value) << "\n";
                break;
            case Option::SPIN:
                std::cout << "spin default " << std::get<int>(opt.value)
                          << " min " << opt.min << " max " << opt.max << "\n";
                break;
            case Option::STRING:
                std::cout << "string default " << std::get<std::string>(opt.value) << "\n";
                break;
            case Option::COMBO:
                std::cout << "combo default " << std::get<std::string>(opt.value);
                for (const auto& var : opt.vars)
                    std::cout << " var " << var;
                std::cout << "\n";
                break;
            case Option::BUTTON:
                std::cout << "button\n";
                break;
        }
    }
}

}  // namespace UCIOptions
