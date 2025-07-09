#include "ucioptions.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace UCIOptions {

// Global options map
std::unordered_map<std::string, Option> options;

// Constructors with callback support
Option::Option(Type t, int def, int min_, int max_, std::function<void(const Option&)> cb) :
    type(t),
    min(min_),
    max(max_),
    value(def),
    on_change(std::move(cb)) {}

Option::Option(Type t, const std::string& def, std::function<void(const Option&)> cb) :
    type(t),
    default_str(def),
    value(def),
    on_change(std::move(cb)) {}

Option::Option(Type                               t,
               const std::string&                 def,
               const std::vector<std::string>&    _options,
               std::function<void(const Option&)> cb) :
    type(t),
    default_str(def),
    vars(_options),
    value(def),
    on_change(std::move(cb)) {}

// Adders with optional callbacks
void addSpin(const std::string&                 name,
             int                                defaultValue,
             int                                min,
             int                                max,
             std::function<void(const Option&)> cb) {
    options[name] = Option(Option::SPIN, defaultValue, min, max, std::move(cb));
    if (options[name].on_change)
        options[name].on_change(options[name]);
}

void addCheck(const std::string& name, bool defaultValue, std::function<void(const Option&)> cb) {
    options[name] = Option(Option::CHECK, defaultValue ? 1 : 0, 0, 1, std::move(cb));
    if (options[name].on_change)
        options[name].on_change(options[name]);
}

void addString(const std::string&                 name,
               const std::string&                 defaultValue,
               std::function<void(const Option&)> cb) {
    options[name] = Option(Option::STRING, defaultValue, std::move(cb));
    if (options[name].on_change)
        options[name].on_change(options[name]);
}

void addCombo(const std::string&                 name,
              const std::string&                 defaultValue,
              const std::vector<std::string>&    vars,
              std::function<void(const Option&)> cb) {
    options[name] = Option(Option::COMBO, defaultValue, vars, std::move(cb));
    if (options[name].on_change)
        options[name].on_change(options[name]);
}

void addButton(const std::string& name, std::function<void(const Option&)> cb) {
    options[name] = Option(Option::BUTTON, "", {}, std::move(cb));
    if (options[name].on_change)
        options[name].on_change(options[name]);
}

void setOption(const std::string& name, const std::string& val) {
    const auto it = options.find(name);
    if (it == options.end())
    {
        std::cerr << "info string Unknown option " << name << '\n';
        return;  // Early-out – nothing to update
    }
    auto& opt = it->second;

    switch (opt.type)
    {
    case Option::CHECK :
        if (val == "true" || val == "1")
            opt.value = 1;
        else if (val == "false" || val == "0")
            opt.value = 0;
        else
        {
            std::cerr << "info string Invalid value for option " << name << ": " << val
                      << " (expected true/false or 1/0)\n";
            return;
        }
        break;
    case Option::SPIN : {
        int v{};
        auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), v);
        if (ec != std::errc() || p != val.data() + val.size())
        {
            std::cerr << "info string Invalid value for option " << name << ": " << val
                      << " (not a valid integer)\n";
            return;
        }
        opt.value = std::clamp(v, opt.min, opt.max);
        break;
    }
    case Option::STRING :
        opt.value = val;
        break;
    case Option::COMBO :
        if (std::find(opt.vars.begin(), opt.vars.end(), val) != opt.vars.end())
        {
            opt.value = val;
        }
        else
        {
            std::cerr << "info string Illegal value \"" << val << "\" for option " << name << "\n";
            return;
        }
        break;
    case Option::BUTTON :
        // no value change
        break;
    }

    // Invoke the callback if present
    if (opt.on_change)
        opt.on_change(opt);
}

int getInt(const std::string& name) {
    const auto& opt = options.at(name);
    if (auto pval = std::get_if<int>(&opt.value))
        return *pval;
    std::cerr << "info string Option " + name + " is not an integer";
    exit(1);
}

std::string getString(const std::string& name) {
    const auto& opt = options.at(name);
    if (auto pval = std::get_if<std::string>(&opt.value))
        return *pval;
    if (auto pval = std::get_if<int>(&opt.value))
        return std::to_string(*pval);
    return "";
}

void printOptions() {
    for (const auto& [name, opt] : options)
    {
        std::cout << "option name " << name << " type ";
        switch (opt.type)
        {
        case Option::CHECK :
            std::cout << "check default " << (std::get<int>(opt.value) ? "true" : "false") << "\n";
            break;
        case Option::SPIN :
            std::cout << "spin default " << std::get<int>(opt.value) << " min " << opt.min
                      << " max " << opt.max << "\n";
            break;
        case Option::STRING :
            std::cout << "string default " << std::get<std::string>(opt.value) << "\n";
            break;
        case Option::COMBO :
            std::cout << "combo default " << std::get<std::string>(opt.value);
            for (const auto& var : opt.vars)
                std::cout << " var " << var;
            std::cout << "\n";
            break;
        case Option::BUTTON :
            std::cout << "button\n";
            break;
        }
    }
}

}  // namespace UCIOptions
