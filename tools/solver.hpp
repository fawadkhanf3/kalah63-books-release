// Exact alpha-beta solver (remaining-seed margin, negamax with no sign change after
// an extra turn), backed by the endgame database, plus a depth-limited heuristic
// alpha-beta used as a ladder opponent.
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "endgame.hpp"
#include "kalah.hpp"

namespace kz {

struct BudgetExceeded {};

class Solver {
 public:
  explicit Solver(const EndgameDB* db, int tt_bits = 22) : db_(db) {
    tt_.assign(size_t(1) << tt_bits, Entry{});
    mask_ = (uint64_t(1) << tt_bits) - 1;
  }

  void clear() { std::fill(tt_.begin(), tt_.end(), Entry{}); }
  uint64_t nodes() const { return nodes_; }

  // Exact final margin (own store - opp store at game end) for the mover, or false if
  // the node budget ran out.
  bool solve(const State& s, uint64_t budget, int& out) {
    if (is_terminal(s)) { out = store_diff(s) + own_pits(s) - opp_pits(s); return true; }
    nodes_ = 0; budget_ = budget;
    try {
      int R = pit_seeds(s);
      out = store_diff(s) + ab(s, -R - 1, R + 1);
      return true;
    } catch (BudgetExceeded&) { return false; }
  }

  // Exact final margin (from the mover's point of view) after playing move a.
  bool move_value(const State& s, int a, uint64_t budget, int& out) {
    MoveResult r = play(s, a);
    int base = store_diff(s) + r.delta;
    if (r.terminal) { out = base; return true; }
    nodes_ = 0; budget_ = budget;
    try {
      int R = pit_seeds(r.next);
      int w = ab(r.next, -R - 1, R + 1);
      out = base + (r.extra ? w : -w);
      return true;
    } catch (BudgetExceeded&) { return false; }
  }

 private:
  struct Entry { uint64_t k1 = 0, k2 = 0; int8_t lo = -127, hi = 127; uint8_t used = 0; };
  const EndgameDB* db_;
  std::vector<Entry> tt_;
  uint64_t mask_, nodes_ = 0, budget_ = 0;

  static void key(const State& s, uint64_t& k1, uint64_t& k2) {
    k1 = 0; k2 = 0;
    for (int i = 0; i < 6; ++i) k1 = (k1 << 6) | s.b[i];
    for (int i = 7; i < 13; ++i) k2 = (k2 << 6) | s.b[i];
  }
  static uint64_t mix(uint64_t a, uint64_t b) {
    uint64_t h = a * 0x9E3779B97F4A7C15ULL ^ (b + 0x632BE59BD9B4E019ULL + (a << 6) + (a >> 2));
    h ^= h >> 31; h *= 0xBF58476D1CE4E5B9ULL; h ^= h >> 29;
    return h;
  }

  int ab(const State& s, int alpha, int beta) {
    if (++nodes_ > budget_) throw BudgetExceeded{};
    int R = pit_seeds(s);
    if (db_ && db_->loaded() && R <= db_->K) return db_->margin(s);
    uint64_t k1, k2; key(s, k1, k2);
    Entry& e = tt_[mix(k1, k2) & mask_];
    bool hit = e.used && e.k1 == k1 && e.k2 == k2;
    if (hit) {
      if (e.lo == e.hi) return e.lo;
      if (e.lo >= beta) return e.lo;
      if (e.hi <= alpha) return e.hi;
      alpha = std::max(alpha, int(e.lo));
      beta = std::min(beta, int(e.hi));
    }
    int a0 = alpha;
    // order: extra turns, then captures/gains, then the rest
    struct M { int a, pri; MoveResult r; };
    M ms[6]; int n = 0;
    int legal = legal_mask(s);
    for (int a = 5; a >= 0; --a) {
      if (!(legal >> a & 1)) continue;
      MoveResult r = play(s, a);
      int pri = r.terminal ? 50 + r.delta : (r.extra ? 100 : r.delta);
      ms[n++] = M{a, pri, r};
    }
    std::sort(ms, ms + n, [](const M& x, const M& y) { return x.pri > y.pri; });
    int best = -1000;
    for (int i = 0; i < n; ++i) {
      const MoveResult& r = ms[i].r;
      int v;
      if (r.terminal) v = r.delta;
      else if (r.extra) v = r.delta + ab(r.next, alpha - r.delta, beta - r.delta);
      else v = r.delta - ab(r.next, r.delta - beta, r.delta - alpha);
      if (v > best) best = v;
      if (best > alpha) alpha = best;
      if (alpha >= beta) break;
    }
    // store / merge bounds
    if (!hit) { e = Entry{}; e.k1 = k1; e.k2 = k2; e.used = 1; }
    if (best <= a0) e.hi = int8_t(std::min(int(e.hi), best));
    else if (best >= beta) e.lo = int8_t(std::max(int(e.lo), best));
    else { e.lo = e.hi = int8_t(best); }
    return best;
  }
};

// Depth-limited heuristic alpha-beta (evaluation = store difference). Extra turns do
// not consume depth. Deterministic: ties go to the lowest pit.
class HeuristicAB {
 public:
  uint64_t nodes = 0;
  int best_move(const State& s, int depth) {
    nodes = 0;
    int best = -100000, bm = -1;
    int legal = legal_mask(s);
    for (int a = 0; a < 6; ++a) {
      if (!(legal >> a & 1)) continue;
      int v = child(s, a, depth, -100000, 100000);
      if (v > best) { best = v; bm = a; }
    }
    return bm;
  }

 private:
  int child(const State& s, int a, int depth, int alpha, int beta) {
    MoveResult r = play(s, a);
    if (r.terminal) return store_diff(r.next) * 1;  // exact final margin
    if (r.extra) return search(r.next, depth, alpha, beta, 0);
    return -search(r.next, depth - 1, -beta, -alpha, 0);
  }
  int search(const State& s, int depth, int alpha, int beta, int chain) {
    ++nodes;
    if (depth <= 0 || chain > 40) return store_diff(s);
    int best = -100000;
    int legal = legal_mask(s);
    for (int a = 0; a < 6; ++a) {
      if (!(legal >> a & 1)) continue;
      MoveResult r = play(s, a);
      int v;
      if (r.terminal) v = store_diff(r.next);
      else if (r.extra) v = search(r.next, depth, alpha, beta, chain + 1);
      else v = -search(r.next, depth - 1, -beta, -alpha, 0);
      if (v > best) best = v;
      if (best > alpha) alpha = best;
      if (alpha >= beta) break;
    }
    return best;
  }
};

}  // namespace kz
