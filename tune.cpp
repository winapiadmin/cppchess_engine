/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tune.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "ucioption.h"

using std::string;

namespace engine {

bool Tune::update_on_last;
const Option *LastOption = nullptr;
OptionsMap *Tune::options;
namespace {
std::map<std::string, int> TuneResults;

std::optional<std::string> on_tune(const Option &o) {

    if (!Tune::update_on_last || LastOption == &o)
        Tune::read_options();

    return std::nullopt;
}
} // namespace

void Tune::make_option(OptionsMap *opts, const string &n, int v, const SetRange &r) {

    // Do not generate option when there is nothing to tune (ie. min = max)
    if (r(v).first == r(v).second)
        return;

    if (TuneResults.count(n))
        v = TuneResults[n];

    opts->add(n, Option(v, r(v).first, r(v).second, on_tune));
    LastOption = &((*opts)[n]);
    auto [a, b] = r(v);
    if (!(a <= v && v <= b)) {
        std::cerr << "wrong bounds, name: " << n << '\n';
        std::exit(1);
    }

    // Print formatted parameters, ready to be copy-pasted in Fishtest
    std::cout << n << "," //
#ifdef OPENBENCH_SUPPORT
                          // or OpenBench
              << "int" << ","
#endif
              << v << ","              //
              << a << ","              //
              << b << ","              //
              << (b - a) / 10.0 << "," //
              << 0.2 << '\n';
}

string Tune::next(string &names, bool pop) {

    string name;

    do {
        string token = names.substr(0, names.find(','));

        if (pop)
            names.erase(0, token.size() + 1);

        std::stringstream ws(token);
        name += (ws >> token, token); // Remove trailing whitespace

    } while (std::count(name.begin(), name.end(), '(') - std::count(name.begin(), name.end(), ')'));

    return name;
}

template <> void Tune::Entry<int>::init_option() { make_option(options, name, value, range); }

template <> void Tune::Entry<int>::read_option() {
    if (options->count(name))
        value = int((*options)[name]);
}

// Instead of a variable here we have a PostUpdate function: just call it
template <> void Tune::Entry<Tune::PostUpdate>::init_option() {}
template <> void Tune::Entry<Tune::PostUpdate>::read_option() { value(); }

template <> void Tune::Entry<int>::print_option(std::ostream &) const {}

template <> void Tune::Entry<Tune::PostUpdate>::print_option(std::ostream &) const {}

namespace {

struct EntryInfo {
    std::string base;
    int value;
    std::vector<int> idx;
};

EntryInfo parse_entry(const std::string &name, int value) {
    EntryInfo info;
    size_t bracket = name.find('[');
    if (bracket == std::string::npos) {
        info.base = name;
        info.value = value;
        return info;
    }
    info.base = name.substr(0, bracket);
    while (bracket != std::string::npos) {
        size_t end = name.find(']', bracket);
        if (end == std::string::npos)
            break;
        info.idx.push_back(std::stoi(name.substr(bracket + 1, end - bracket - 1)));
        bracket = name.find('[', end + 1);
    }
    info.value = value;
    return info;
}

void deduce_shape(const std::vector<EntryInfo> &entries, std::vector<int> &shape) {
    if (entries.empty())
        return;
    int ndim = (int)entries[0].idx.size();
    shape.assign(ndim, 0);
    for (auto &e : entries)
        for (int d = 0; d < ndim; d++)
            if (e.idx[d] >= shape[d])
                shape[d] = e.idx[d] + 1;
}

void print_array(std::ostream &os, const std::vector<int> &flat, const int *shape, int ndim, int dim, int &pos) {
    if (dim == ndim - 1) {
        os << "{ ";
        for (int i = 0; i < shape[dim]; i++) {
            if (i)
                os << ", ";
            os << flat[pos++];
        }
        os << " }";
    } else {
        os << "{ ";
        for (int i = 0; i < shape[dim]; i++) {
            if (i)
                os << ", ";
            print_array(os, flat, shape, ndim, dim + 1, pos);
        }
        os << " }";
    }
}

} // namespace

void Tune::export_weights(std::ostream &os) {
    std::vector<EntryInfo> scalars;
    std::map<std::string, std::vector<EntryInfo>> groups;

    for (auto &e : instance().list) {
        auto entry = dynamic_cast<Entry<int> *>(e.get());
        if (!entry)
            continue;
        auto info = parse_entry(entry->name, entry->value);
        if (info.idx.empty())
            scalars.push_back(info);
        else
            groups[info.base].push_back(info);
    }

    os << "#ifndef WEIGHTS_H\n";
    os << "#define WEIGHTS_H\n";
    os << "#include \"eval.h\"\n";
    os << "namespace engine::eval {\n";

    for (auto &e : scalars)
        os << "inline Value " << e.base << " = " << e.value << ";\n";

    for (auto &[base, entries] : groups) {
        std::vector<int> shape;
        deduce_shape(entries, shape);
        int ndim = (int)shape.size();
        // Build flat buffer, zero-initialized, fill from entries
        int total = 1;
        for (int d = 0; d < ndim; d++)
            total *= shape[d];
        std::vector<int> flat(total, 0);
        for (auto &e : entries) {
            int linear = 0, stride = total;
            for (int d = 0; d < ndim; d++) {
                stride /= shape[d];
                linear += e.idx[d] * stride;
            }
            flat[linear] = e.value;
        }
        os << "inline Value " << base;
        for (int d = 0; d < ndim; d++)
            os << "[" << shape[d] << "]";
        os << " = ";
        int pos = 0;
        print_array(os, flat, shape.data(), ndim, 0, pos);
        os << ";\n";
    }

    os << "} // namespace engine::eval\n";
    os << "#endif\n";
}

} // namespace engine

// Init options with tuning session results instead of default values. Useful to
// get correct bench signature after a tuning session or to test tuned values.
// Just copy fishtest tuning results in a result.txt file and extract the
// values with:
//
// cat results.txt | sed 's/^param: \([^,]*\), best: \([^,]*\).*/
// TuneResults["\1"] = int(round(\2));/'
//
// Then paste the output below, as the function body

namespace engine {

void Tune::read_results() { /* ...insert your values here... */ }

} // namespace engine
