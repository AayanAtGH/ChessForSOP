from config import EVAL_THRESHOLD
from db import ChessGraphDB


def analyse_position(
    db: ChessGraphDB,
    current_fen: str,
    eval_threshold: float = EVAL_THRESHOLD,
):
    """
    Given a FEN, try to find a favourable line in the graph.

    Strategy:
      1. Look for a direct graph path from this exact position.
      2. If none, find structurally similar positions (same pieces/castling)
         and search for lines from those instead.
    """
    print(f"\n{'=' * 60}")
    print(f"Current position : {current_fen}")

    # -- Step 1: exact path from the current node ----------------------
    lines = db.find_lines_to_favourable(current_fen, eval_threshold)

    if lines:
        print(f"\n  Found {len(lines)} direct line(s) to a favourable position:\n")
        for i, line in enumerate(lines, 1):
            moves_str = " ".join(line["moves"])
            print(
                f"  Line {i}: {moves_str}\n"
                f"           → eval {line['target_eval']:+.2f}  "
                f"(in {line['depth']} move(s))"
            )
        return

    # -- Step 2: structurally similar positions ------------------------
    print("\n   No direct path found. Checking structurally similar positions…")
    similar = db.find_similar_favourable(current_fen, eval_threshold)

    if not similar:
        print("\n✗   No similar favourable positions found in the database.")
        return

    found_any = False
    for pos in similar:
        print(f"\n  Similar position (eval {pos['eval']:+.2f}): {pos['fen']}")
        lines = db.find_lines_to_favourable(pos["fen"], eval_threshold)

        if lines:
            found_any = True
            print(f"    Reachable via {len(lines)} line(s) from that position:")
            for line in lines[:2]:
                print(
                    f"      {' '.join(line['moves'])}  "
                    f"(eval {line['target_eval']:+.2f})"
                )
        else:
            print("      ✗  No further path found from this similar position.")

    if not found_any:
        print("\n✗   Could not find a reachable favourable line from any similar position.")
