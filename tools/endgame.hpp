// Exact endgame database for every distribution of <= K seeds over the 12 pits.
// Stores, for the side to move, how many of the remaining pit seeds it secures
// under optimal play (0..R). Store contents never matter for the remaining game,
// so the index uses pits only. Built level by level (R ascending); inside a level
// a memoized recursion is used: a move that puts nothing in a store strictly
// decreases sum(seed * distance-to-own-store), so the recursion cannot cycle.
#pragma once
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "kalah.hpp"

namespace kz {

class EndgameDB {
 public:
  int K = -1;

  void init_tables(int k) {
    K = k;
    // comp[n][parts] = number of ways to put n seeds in `parts` pits
    comp_.assign((K + 1) * 13, 0);
    for (int n = 0; n <= K; ++n) {
      comp_[n * 13 + 1] = 1;
      for (int p = 2; p <= 12; ++p) {
        uint64_t t = 0;
        for (int v = 0; v <= n; ++v) t += comp_[(n - v) * 13 + p - 1];
        comp_[n * 13 + p] = t;
      }
    }
    // cum[n][parts][c] = sum_{v<c} comp[n-v][parts-1]
    cum_.assign((K + 1) * 13 * (K + 2), 0);
    for (int n = 0; n <= K; ++n)
      for (int p = 2; p <= 12; ++p) {
        uint64_t t = 0;
        for (int c = 0; c <= n + 1 && c <= K + 1; ++c) {
          cum_[(n * 13 + p) * (K + 2) + c] = t;
          if (c <= n) t += comp_[(n - c) * 13 + p - 1];
        }
      }
    offset_.assign(K + 2, 0);
    for (int r = 0; r <= K; ++r) offset_[r + 1] = offset_[r] + comp_[r * 13 + 12];
  }

  uint64_t size() const { return offset_.empty() ? 0 : offset_[K + 1]; }
  bool loaded() const { return !table_.empty(); }

  uint64_t index(const State& s, int R) const {
    uint64_t r = offset_[R];
    int n = R;
    int pits[12];
    for (int i = 0; i < 6; ++i) { pits[i] = s.b[i]; pits[6 + i] = s.b[7 + i]; }
    for (int i = 0; i < 11; ++i) {
      int parts = 12 - i, c = pits[i];
      r += cum_[(n * 13 + parts) * (K + 2) + c];
      n -= c;
    }
    return r;
  }

  // Remaining-seed margin for the mover: (seeds mover secures) - (seeds opponent secures).
  int margin(const State& s) const {
    int R = pit_seeds(s);
    return 2 * int(table_[index(s, R)]) - R;
  }

  void build(int k, bool verbose = true) {
    init_tables(k);
    table_.assign(size(), 255);
    for (int R = 0; R <= K; ++R) {
      State s;  // stores stay zero during the build
      int pits[12] = {0};
      enumerate(pits, 0, R, s);
      if (verbose) { std::printf("endgame level %d/%d done (%llu entries)\n", R, K, (unsigned long long)offset_[R + 1]); std::fflush(stdout); }
    }
  }

  void save(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) throw std::runtime_error("cannot write " + path);
    const char magic[8] = {'K', 'Z', 'E', 'G', 'D', 'B', '0', '1'};
    uint64_t n = size();
    int32_t k = K;
    std::fwrite(magic, 1, 8, f);
    std::fwrite(&k, 4, 1, f);
    std::fwrite(&n, 8, 1, f);
    std::fwrite(table_.data(), 1, n, f);
    std::fclose(f);
  }

  void load(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot read " + path);
    char magic[8];
    int32_t k;
    uint64_t n;
    if (std::fread(magic, 1, 8, f) != 8 || std::memcmp(magic, "KZEGDB01", 8) != 0) { std::fclose(f); throw std::runtime_error("bad endgame db magic"); }
    if (std::fread(&k, 4, 1, f) != 1 || std::fread(&n, 8, 1, f) != 1) { std::fclose(f); throw std::runtime_error("bad endgame db header"); }
    init_tables(k);
    if (n != size()) { std::fclose(f); throw std::runtime_error("endgame db size mismatch"); }
    table_.resize(n);
    if (std::fread(table_.data(), 1, n, f) != n) { std::fclose(f); throw std::runtime_error("truncated endgame db"); }
    std::fclose(f);
  }

 private:
  std::vector<uint64_t> comp_, cum_, offset_;
  std::vector<uint8_t> table_;

  void enumerate(int* pits, int i, int left, State& s) {
    if (i == 11) {
      pits[11] = left;
      for (int j = 0; j < 6; ++j) { s.b[j] = uint8_t(pits[j]); s.b[7 + j] = uint8_t(pits[6 + j]); }
      s.b[6] = s.b[13] = 0;
      solve(s);
      return;
    }
    for (int v = 0; v <= left; ++v) { pits[i] = v; enumerate(pits, i + 1, left - v, s); }
  }

  int solve(const State& s0) {
    State s = s0;
    s.b[6] = s.b[13] = 0;
    int R = pit_seeds(s);
    uint64_t idx = index(s, R);
    if (table_[idx] != 255) return 2 * int(table_[idx]) - R;
    int best;
    if (own_pits(s) == 0 || opp_pits(s) == 0) {
      // Only reachable as a stored position (not via play): game already over.
      best = own_pits(s) - opp_pits(s);
    } else {
      best = -1000;
      int mask = legal_mask(s);
      for (int a = 0; a < 6; ++a) {
        if (!(mask >> a & 1)) continue;
        MoveResult r = play(s, a);
        int v;
        if (r.terminal) v = r.delta;
        else if (r.extra) v = r.delta + solve(r.next);
        else v = r.delta - solve(r.next);
        if (v > best) best = v;
      }
    }
    table_[idx] = uint8_t((best + R) / 2);
    return best;
  }
};

}  // namespace kz
