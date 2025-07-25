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

#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

namespace Stockfish {  // misc.h, /IsLittleEndian
// True if and only if the binary is compiled on a little-endian machine
static inline const std::uint16_t Le             = 1;
static inline const bool          IsLittleEndian = *reinterpret_cast<const char*>(&Le) == 1;
template<typename T, std::size_t MaxSize>
class ValueList {

   public:
    std::size_t size() const { return size_; }
    void        push_back(const T& value) { values_[size_++] = value; }
    const T*    begin() const { return values_; }
    const T*    end() const { return values_ + size_; }
    const T&    operator[](int index) const { return values_[index]; }

   private:
    T           values_[MaxSize];
    std::size_t size_ = 0;
};

#if defined(__GNUC__) && !defined(__clang__)
    #if __GNUC__ >= 13
        #define sf_assume(cond) __attribute__((assume(cond)))
    #else
        #define sf_assume(cond) \
            do \
            { \
                if (!(cond)) \
                    __builtin_unreachable(); \
            } while (0)
    #endif
#else
    // do nothing for other compilers
    #define sf_assume(cond)
#endif

#define sync_cout std::cout
#define sync_endl std::endl

}  // namespace Stockfish

#endif  // #ifndef MISC_H_INCLUDED
