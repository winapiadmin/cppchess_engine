#pragma once
#include <algorithm>
#include <cstring>
#include <types.h>
namespace engine {
enum TTFlag : uint8_t { EXACT = 0, LOWERBOUND = 1, UPPERBOUND = 2 };

struct TTEntry {
  uint64_t key;
  uint64_t pack; // 16-bit score, 8-bit depth, 3-bit flags, 16-bit move, 21 bits
                 // for generation

  // bit layout constants
  static constexpr unsigned SCORE_SHIFT = 0;
  static constexpr unsigned SCORE_BITS = 16;
  static constexpr uint64_t SCORE_MASK = ((uint64_t(1) << SCORE_BITS) - 1)
                                         << SCORE_SHIFT;

  static constexpr unsigned DEPTH_SHIFT = 16;
  static constexpr unsigned DEPTH_BITS = 8;
  static constexpr uint64_t DEPTH_MASK = ((uint64_t(1) << DEPTH_BITS) - 1)
                                         << DEPTH_SHIFT;

  static constexpr unsigned FLAG_SHIFT = 24;
  static constexpr unsigned FLAG_BITS = 3;
  static constexpr uint64_t FLAG_MASK = ((uint64_t(1) << FLAG_BITS) - 1)
                                        << FLAG_SHIFT;

  static constexpr unsigned MOVE_SHIFT = 27;
  static constexpr unsigned MOVE_BITS = 16;
  static constexpr uint64_t MOVE_MASK = ((uint64_t(1) << MOVE_BITS) - 1)
                                        << MOVE_SHIFT;

  static constexpr unsigned GEN_SHIFT = 43;
  static constexpr unsigned GEN_BITS = 21;
  static constexpr uint64_t GEN_MASK = ((uint64_t(1) << GEN_BITS) - 1)
                                       << GEN_SHIFT;

  // getters
  inline uint16_t getScore() const noexcept {
    return static_cast<uint16_t>((pack & SCORE_MASK) >> SCORE_SHIFT);
  }

  inline uint8_t getDepth() const noexcept {
    return static_cast<uint8_t>((pack & DEPTH_MASK) >> DEPTH_SHIFT);
  }

  inline TTFlag getFlag() const noexcept {
    return static_cast<TTFlag>((pack & FLAG_MASK) >> FLAG_SHIFT);
  }

  inline uint16_t getMove() const noexcept {
    return static_cast<uint16_t>((pack & MOVE_MASK) >> MOVE_SHIFT);
  }

  inline uint32_t getGeneration() const noexcept {
    return static_cast<uint32_t>((pack & GEN_MASK) >> GEN_SHIFT);
  }

  // setters
  inline void setScore(int16_t score) noexcept {
    // preserve two's complement by casting through uint16_t
    const uint64_t v =
        (static_cast<uint64_t>(static_cast<uint16_t>(score)) << SCORE_SHIFT) &
        SCORE_MASK;
    pack = (pack & ~SCORE_MASK) | v;
  }

  inline void setDepth(uint8_t depth) noexcept {
    const uint64_t v =
        (static_cast<uint64_t>(depth) << DEPTH_SHIFT) & DEPTH_MASK;
    pack = (pack & ~DEPTH_MASK) | v;
  }

  inline void setFlag(TTFlag flag) noexcept {
    const uint64_t v =
        (static_cast<uint64_t>(static_cast<uint8_t>(flag)) << FLAG_SHIFT) &
        FLAG_MASK;
    pack = (pack & ~FLAG_MASK) | v;
  }

  inline void setMove(uint16_t move) noexcept {
    const uint64_t v = (static_cast<uint64_t>(move) << MOVE_SHIFT) & MOVE_MASK;
    pack = (pack & ~MOVE_MASK) | v;
  }

  inline void setGeneration(uint32_t gen) noexcept {
    const uint64_t v = (static_cast<uint64_t>(gen) << GEN_SHIFT) & GEN_MASK;
    pack = (pack & ~GEN_MASK) | v;
  }

  // convenience: set all packed fields at once
  inline void setPackedFields(int16_t score, uint8_t depth, TTFlag flag,
                              uint16_t move, uint32_t gen) noexcept {
    const uint64_t s =
        (static_cast<uint64_t>(static_cast<uint16_t>(score)) << SCORE_SHIFT) &
        SCORE_MASK;
    const uint64_t d =
        (static_cast<uint64_t>(depth) << DEPTH_SHIFT) & DEPTH_MASK;
    const uint64_t f =
        (static_cast<uint64_t>(static_cast<uint8_t>(flag)) << FLAG_SHIFT) &
        FLAG_MASK;
    const uint64_t m = (static_cast<uint64_t>(move) << MOVE_SHIFT) & MOVE_MASK;
    const uint64_t g = (static_cast<uint64_t>(gen) << GEN_SHIFT) & GEN_MASK;
    pack = s | d | f | m | g;
  }

  inline uint32_t timestamp() const noexcept { return getGeneration(); }
};
class TranspositionTable {
  TTEntry *table;
  int buckets; // number of buckets (pairs)
  uint32_t time;

public:
  int size; // total number of TTEntry elements (must be even)
  TranspositionTable() : table(nullptr), buckets(0), time(0), size(0) {}

  TranspositionTable(int sizeInMB) : time(0) {
    size = sizeInMB * 1048576 / sizeof(TTEntry);
    if (size % 2 != 0)
      size--; // Ensure even size
    buckets = size / 2;
    table = new TTEntry[size];
    clear();
  }

  ~TranspositionTable() { delete[] table; }

  void resize(int sizeInMB) {
    TTEntry *old_table = table;
    int old_size = size;

    size = sizeInMB * 1048576 / sizeof(TTEntry);
    if (size % 2 != 0)
      size--;
    buckets = size / 2;

    TTEntry *new_table = new (std::nothrow) TTEntry[size];
    if (!new_table) {
      // Restore old values on failure
      table = old_table;
      size = old_size;
      buckets = old_size / 2;
      throw std::bad_alloc();
    }

    std::memcpy(new_table, old_table,
                sizeof(TTEntry) * std::min(size, old_size));
    delete[] old_table;
    table = new_table;
    buckets = size / 2;
  }

  void newSearch();
  void store(uint64_t hash, chess::Move best, int16_t score, int8_t depth,
             TTFlag flag);

  inline void clear() { std::fill_n(table, size, TTEntry{}); }

  TTEntry *lookup(uint64_t hash);

  // Returns hash usage as percentage [0..100] based on occupied buckets.
  inline int hashfull() const noexcept {
    if (!table || buckets <= 0)
      return 0;

    int used = 0;
    for (int i = 0; i < buckets; ++i) {
      const TTEntry &a = table[2 * i];
      const TTEntry &b = table[2 * i + 1];
      if (a.key != 0 || b.key != 0)
        ++used;
    }

    return (used * 1000) / buckets;
  }
};
} // namespace engine