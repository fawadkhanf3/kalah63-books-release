// Standalone exact tools for Kalah(6,n), standard capture rule (Solver B of the paper).
//   build-db <K> <file>                    build the endgame database for <= K pit seeds
//   value <file> <seeds>                   exact value of the start position of Kalah(6,seeds)
//   moves <file> b0 b1 ... b13             exact final margin of every legal move; the board is
//                                          mover-relative: b0..b5 mover's pits (sowing order),
//                                          b6 mover's store, b7..b12 opponent's pits, b13 its store
// Build:  g++ -O2 -std=c++17 -o kalah_tool kalah_tool.cpp
#include <cstdio>
#include <cstdlib>
#include <string>
#include "kalah.hpp"
#include "endgame.hpp"
#include "solver.hpp"
using namespace kz;

static int move_value(Solver& sol, const EndgameDB& db, const State& s, int a, bool& ok) {
  MoveResult r = play(s, a);
  int base = store_diff(s) + r.delta;
  ok = true;
  if (r.terminal) return base;
  if (pit_seeds(r.next) <= db.K) { int w = db.margin(r.next); return base + (r.extra ? w : -w); }
  int v; ok = sol.move_value(s, a, ~0ULL, v); return v;
}

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: see header of kalah_tool.cpp\n"); return 2; }
  std::string cmd = argv[1];
  if (cmd == "build-db" && argc == 4) {
    EndgameDB db; db.build(std::atoi(argv[2])); db.save(argv[3]); return 0;
  }
  EndgameDB db; db.load(argv[2]);
  Solver sol(&db, 24);
  if (cmd == "value" && argc == 4) {
    State s = initial(std::atoi(argv[3])); int v;
    if (!sol.solve(s, ~0ULL, v)) return 1;
    std::printf("Kalah(6,%s) value for the first player: %+d (nodes %llu)\n", argv[3], v, (unsigned long long)sol.nodes());
    return 0;
  }
  if (cmd == "moves" && argc == 17) {
    State s; for (int i = 0; i < 14; ++i) s.b[i] = uint8_t(std::atoi(argv[3 + i]));
    for (int a = 0; a < 6; ++a) {
      if (!s.b[a]) { std::printf("pit %d: illegal\n", a + 1); continue; }
      bool ok; int v = move_value(sol, db, s, a, ok);
      std::printf("pit %d: %+d\n", a + 1, v);
    }
    return 0;
  }
  std::fprintf(stderr, "bad arguments\n"); return 2;
}
