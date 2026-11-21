#!/usr/bin/env python3
"""Verify the Kalah(6,3) perfect-play strategy books (standard capture rule).

Standard library only. Usage:

    python verify_books.py kalah-6x3-starting-strategy.bin kalah-6x3-second-strategy.bin

Rules (Kalah(6,3), standard capture rule)
  Board: absolute indices 0-5 are Player 1's pits (left to right from Player 1's side),
  6 is Player 1's store, 7-12 are Player 2's pits, 13 is Player 2's store. Each pit
  starts with 3 seeds. A move lifts all seeds from one of the mover's non-empty pits and
  sows them counter-clockwise, one per pit, skipping the opponent's store. If the last
  seed lands in the mover's store, the mover moves again. If the last seed lands in an
  empty pit on the mover's side AND the opposite pit is non-empty, both pits are emptied
  into the mover's store (no capture if the opposite pit is empty). The game ends when
  either row is empty; each player then adds the seeds left on their side to their store.

Book format (K63BOOK1)
  16-byte header: magic b"K63BOOK1" + uint64 record count (little endian), then records
  of 17 bytes sorted by key: uint64 hi, uint64 lo, uint8 move. lo packs absolute board
  cells 0..6 one byte each (cell i in byte i), hi packs cells 7..13. The move is the
  mover's pit 0..5, counted from the mover's left (pit index = move for Player 1,
  7 + move for Player 2). The first book holds positions with Player 1 to move, the
  second with Player 2 to move.

What is checked
  1. Structure: magic, record count, strictly increasing keys (no duplicates), every
     board holds 36 seeds and is not a finished game, every move is legal.
  2. Certificate: the book's complete strategy tree is walked from the start position:
     the book move wherever the book's side is to move, EVERY legal reply wherever the
     opponent is to move, through extra turns, until every line ends in a finished game.
     No endgame database or search is used. The walk must never meet a position missing
     from the book, and its worst-case final margin (Player 1 minus Player 2) must be
     +2 for the first-player book (Player 1 forces at least +2) and +2 for the
     second-player book (Player 2 holds Player 1 to at most +2). Together these prove
     that the game value is exactly +2.
"""
from __future__ import annotations
import struct
import sys
import time

STORE = (6, 13)
PITS = (range(0, 6), range(7, 13))


def read_book(path):
    data = open(path, "rb").read()
    magic, count = struct.unpack_from("<8sQ", data, 0)
    assert magic == b"K63BOOK1", f"{path}: bad magic {magic!r}"
    assert len(data) == 16 + 17 * count, f"{path}: size does not match record count"
    book, prev = {}, None
    for i in range(count):
        hi, lo, move = struct.unpack_from("<QQB", data, 16 + 17 * i)
        key = (hi, lo)
        assert prev is None or key > prev, f"{path}: record {i} out of order or duplicate"
        prev = key
        book[key] = move
    return book


def unpack(hi, lo):
    return [(lo >> (8 * i)) & 255 for i in range(7)] + [(hi >> (8 * i)) & 255 for i in range(7)]


def pack(board):
    lo = sum(board[i] << (8 * i) for i in range(7))
    hi = sum(board[7 + i] << (8 * i) for i in range(7))
    return hi, lo


def finished(board):
    return all(board[i] == 0 for i in PITS[0]) or all(board[i] == 0 for i in PITS[1])


def play(board, player, move):
    """Returns (new_board, next_player, game_over)."""
    pit = PITS[player][move]
    b = board[:]
    seeds, b[pit], pos = b[pit], 0, pit
    while seeds:
        pos = (pos + 1) % 14
        if pos == STORE[1 - player]:
            continue
        b[pos] += 1
        seeds -= 1
    if pos in PITS[player] and b[pos] == 1 and b[12 - pos] > 0:
        b[STORE[player]] += b[pos] + b[12 - pos]
        b[pos] = b[12 - pos] = 0
    if finished(b):
        for side in (0, 1):
            for i in PITS[side]:
                b[STORE[side]] += b[i]
                b[i] = 0
        return b, None, True
    return b, (player if pos == STORE[player] else 1 - player), False


def check_structure(book, player):
    for (hi, lo), move in book.items():
        b = unpack(hi, lo)
        assert sum(b) == 36, f"seed count {sum(b)} in {b}"
        assert not finished(b), f"finished position stored: {b}"
        assert 0 <= move < 6 and b[PITS[player][move]] > 0, f"illegal move {move} in {b}"


def certify(book, seat):
    start = [3] * 6 + [0] + [3] * 6 + [0]
    seen = set()
    stack = [(start, 0)]
    worst = None
    used = set()
    while stack:
        b, p = stack.pop()
        k = (tuple(b), p)
        if k in seen:
            continue
        seen.add(k)
        if p == seat:
            key = pack(b)
            if key not in book:
                raise AssertionError(f"position missing from book: {b} (player {p + 1} to move)")
            used.add(key)
            moves = [book[key]]
        else:
            moves = [m for m in range(6) if b[PITS[p][m]] > 0]
        for m in moves:
            nb, np_, over = play(b, p, m)
            if over:
                margin = nb[6] - nb[13]                      # Player 1 minus Player 2
                if worst is None or (margin < worst if seat == 0 else margin > worst):
                    worst = margin
            else:
                stack.append((nb, np_))
    return worst, len(seen), len(used)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    ok = True
    for seat, path in enumerate(sys.argv[1:]):
        t = time.time()
        book = read_book(path)
        check_structure(book, seat)
        worst, positions, used = certify(book, seat)
        bound = "at least" if seat == 0 else "at most"
        good = worst == 2 and used == len(book)
        ok &= good
        print(f"{path}: {len(book):,} records, structure OK; strategy tree {positions:,} positions, "
              f"{used:,} records reached; Player 1's margin is {bound} {worst:+d} "
              f"against every opponent ({time.time() - t:.0f} s) -> {'OK' if good else 'FAILED'}")
    print("Kalah(6,3) standard rule: value exactly +2 CERTIFIED" if ok else "CERTIFICATE FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
