# Kalah(6,3) perfect-play strategy books (standard capture rule)

Companion data for: F. F. Ashraf, "Solving Kalah(6,3) Under the Standard Capture Rule and
Measuring Tabula-Rasa Self-Play Against Perfect Play."

Kalah(6,3) under the standard capture rule (no capture when the opposite pit is empty) is a
first-player win by two seeds, and pit 5 is the only optimal opening.

| File | Contents |
|---|---|
| `kalah-6x3-starting-strategy.bin` | 308,782 positions with Player 1 to move; guarantees Player 1 at least +2 |
| `kalah-6x3-second-strategy.bin` | 601,427 positions with Player 2 to move; holds Player 1 to at most +2 |
| `verify_books.py` | One-command certificate (Python 3.8+, standard library only) |
| `exploit_tree.py` | Exact exploitability (worst case against a best response) of any deterministic strategy (standard library only) |
| `probe_set.csv` | The 2,000 held-out probe positions of the paper with the exact final margin of every legal move |
| `tools/` | C++17 endgame-database builder and exact solver (Solver B): `kalah_tool.cpp` plus three headers |
| `SHA256SUMS` | Checksums of the files above |

## Verify

```
python verify_books.py kalah-6x3-starting-strategy.bin kalah-6x3-second-strategy.bin
```

Expected output (about 15 s):

```
kalah-6x3-starting-strategy.bin: 308,782 records, structure OK; strategy tree 521,719 positions, 308,782 records reached; Player 1's margin is at least +2 against every opponent -> OK
kalah-6x3-second-strategy.bin: 601,427 records, structure OK; strategy tree 1,004,436 positions, 601,427 records reached; Player 1's margin is at most +2 against every opponent -> OK
Kalah(6,3) standard rule: value exactly +2 CERTIFIED
```

The script walks each book's complete strategy tree against every opponent reply until every
line ends in a finished game, using only the rules (no endgame database, no search). Every
record of both books is reached. The rules, board layout and file format are documented at the
top of `verify_books.py`.

Every stored move preserves the game value. Every stored move is also margin-optimal except
two positions (one per book, both reachable only after an opponent error), where the stored
move wins or loses 4 seeds less favorably than the best move while still keeping the value.

## Use in a program

Look up the absolute board (Player 1's pits and store in bytes 0-6 of `lo`, Player 2's in
bytes 0-6 of `hi`) with a binary search over the sorted 17-byte records of the book for the
side to move; the stored byte is the pit to play, counted 0-5 from that player's left.

## Probe set

`probe_set.csv` has one row per position: `b0`..`b13` is the board relative to the player to
move (`b0`..`b5` that player's pits in sowing order, `b6` its store, `b7`..`b12` the opponent's
pits, `b13` the opponent's store), followed by the exact final margin, for the player to move,
of each move (empty for an illegal move). Every position has at most 18 seeds in the pits and at
least two legal moves. A move's regret is the row maximum minus its value.

## Exploitability

```
python exploit_tree.py kalah-6x3-starting-strategy.bin kalah-6x3-second-strategy.bin
```

prints worst cases of exactly +2 and -2 for the books. Import `worst_case(policy, seat)` to
evaluate any deterministic agent.

## Endgame database and exact solver

```
g++ -O2 -std=c++17 -o kalah_tool tools/kalah_tool.cpp
./kalah_tool build-db 18 endgame_le18.kzdb        # about 45 s, 86 MB
./kalah_tool value endgame_le18.kzdb 3            # Kalah(6,3) value for the first player: +2
./kalah_tool moves endgame_le18.kzdb 3 3 3 3 3 3 0 3 3 3 3 3 3 0
```

The last command prints the exact value of each opening: -14, -16, -10, -2, +2, 0 for pits 1-6.
