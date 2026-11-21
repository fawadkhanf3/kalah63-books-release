// Kalah(6,n) rules, non-empty-capture variant.
// Boards are always MOVER-RELATIVE: indices 0..5 are the mover's pits (sowing order),
// 6 is the mover's store, 7..12 the opponent's pits, 13 the opponent's store.
// This is exactly the absolute board when player 0 is to move; for player 1 the
// absolute board is this array rotated by 7.
#pragma once
#include <array>
#include <cstdint>

namespace kz {

struct State {
  std::array<uint8_t, 14> b{};
};

struct MoveResult {
  State next;       // mover-relative for the NEXT player to move (flipped unless extra
                    // turn); for terminal positions orientation of the player who moved
  bool extra = false;
  bool terminal = false;
  int delta = 0;    // change of (own store - opp store) for the player who moved,
                    // including captures and, if terminal, the final sweep
};

inline State flip(const State& s) {
  State n;
  for (int i = 0; i < 14; ++i) n.b[i] = s.b[(i + 7) % 14];
  return n;
}

inline State initial(int seeds) {
  State s;
  for (int i = 0; i < 14; ++i) s.b[i] = uint8_t(seeds);
  s.b[6] = s.b[13] = 0;
  return s;
}

inline int own_pits(const State& s) { int t = 0; for (int i = 0; i < 6; ++i) t += s.b[i]; return t; }
inline int opp_pits(const State& s) { int t = 0; for (int i = 7; i < 13; ++i) t += s.b[i]; return t; }
inline int pit_seeds(const State& s) { return own_pits(s) + opp_pits(s); }
inline int store_diff(const State& s) { return int(s.b[6]) - int(s.b[13]); }
inline bool is_terminal(const State& s) { return own_pits(s) == 0 || opp_pits(s) == 0; }

inline int legal_mask(const State& s) {
  int m = 0;
  for (int i = 0; i < 6; ++i) if (s.b[i]) m |= 1 << i;
  return m;
}

inline MoveResult play(const State& s, int a) {
  MoveResult r;
  State n = s;
  int hand = n.b[a];
  n.b[a] = 0;
  int c = a;
  while (hand) {
    c = (c + 1) % 14;
    if (c == 13) continue;  // skip the opponent's store
    n.b[c]++;
    --hand;
  }
  if (c < 6 && n.b[c] == 1 && n.b[12 - c] > 0) {  // non-empty capture
    n.b[6] = uint8_t(n.b[6] + 1 + n.b[12 - c]);
    n.b[c] = 0;
    n.b[12 - c] = 0;
  }
  if (own_pits(n) == 0 || opp_pits(n) == 0) {
    int mine = own_pits(n), theirs = opp_pits(n);
    for (int i = 0; i < 6; ++i) n.b[i] = 0;
    for (int i = 7; i < 13; ++i) n.b[i] = 0;
    n.b[6] = uint8_t(n.b[6] + mine);
    n.b[13] = uint8_t(n.b[13] + theirs);
    r.terminal = true;
    r.delta = store_diff(n) - store_diff(s);
    r.next = n;
    return r;
  }
  r.delta = store_diff(n) - store_diff(s);
  r.extra = (c == 6);
  r.next = r.extra ? n : flip(n);
  return r;
}

}  // namespace kz
