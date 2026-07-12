#!/usr/bin/env python3

import argparse
import glob
from collections import defaultdict
from itertools import chain
import chess.pgn

# [LL, LD/DL, WL/DD/LW, WD/DW, WW]
PTNML_INDEX = {-2: 0, -1: 1, 0: 2, 1: 3, 2: 4}


def score_from_new(headers):
    result = headers["Result"]

    if result == "1/2-1/2":
        return 0

    new_is_white = headers["White"] == "new"

    if new_is_white:
        return 1 if result == "1-0" else -1
    else:
        return 1 if result == "0-1" else -1


def main():
    parser = argparse.ArgumentParser(
        description="Extract WDL and pentanomial statistics from fastchess PGNs"
    )
    parser.add_argument("pgns", nargs="+")
    args = parser.parse_args()

    pairs = defaultdict(list)

    # WDL = [loss, draw, win]
    wdl = [0, 0, 0]
    games = 0

    for filename in chain.from_iterable(glob.glob(p) for p in args.pgns):
        with open(filename, encoding="utf-8") as f:
            while (game := chess.pgn.read_game(f)) is not None:
                headers = game.headers

                score = score_from_new(headers)

                games += 1
                if score < 0:
                    wdl[0] += 1
                elif score == 0:
                    wdl[1] += 1
                else:
                    wdl[2] += 1

                fen = headers.get("FEN")
                round_id = headers.get("Round")

                if fen is None or round_id is None:
                    raise RuntimeError(
                        f"{filename}: missing FEN or Round tag"
                    )

                # Include filename because each shard starts rounds again
                key = (
                    filename,
                    round_id,
                    fen,
                )

                pairs[key].append(score)

    ptnml = [0] * 5
    pairs_count = 0

    for key, scores in pairs.items():
        if len(scores) != 2:
            print(
                f"Skipping incomplete pair {key}: got {len(scores)} games"
            )
            continue

        # Make sure the two games are the repeat pair
        if scores[0] + scores[1] not in PTNML_INDEX:
            raise RuntimeError("Invalid pair score")

        ptnml[PTNML_INDEX[scores[0] + scores[1]]] += 1
        pairs_count += 1

    print(f"Games: {games}")
    print(f"Pairs: {pairs_count}")

    print()
    print("WDL:")
    print(f"  Loss: {wdl[0]}")
    print(f"  Draw: {wdl[1]}")
    print(f"  Win : {wdl[2]}")
    print()
    print(wdl)

    print()
    print("PTNML:")
    names = [
        "LL",
        "LD",
        "WL/DD",
        "WD",
        "WW",
    ]

    for name, count in zip(names, ptnml):
        print(f"  {name:10}: {count}")

    print()
    print(ptnml)
    

if __name__ == "__main__":
    main()