import chess

from config import (
    NEO4J_URI,
    NEO4J_USER,
    NEO4J_PASSWORD,
    STOCKFISH_PATH,
    EVAL_DEPTH,
    INGEST_LIMIT,
)
from db import ChessGraphDB
from analysis import analyse_position


def ingest_games(db: ChessGraphDB, pgn_path: str, limit: int = INGEST_LIMIT):
    """Read up to `limit` games from a PGN file and ingest them into Neo4j."""
    print(f"Ingesting up to {limit} games from {pgn_path} …")
    ingested = 0

    with open(pgn_path) as f:
        # while ingested < limit:
        #     # chess.pgn.read_game reads one game at a time
        #     import chess.pgn
        #     game = chess.pgn.read_game(f)
        #     if game is None:
        #         break

        #     import io
        #     exporter = io.StringIO()
        #     game.accept(chess.pgn.StringExporter(headers=True, variations=False))
        #     db.ingest_pgn(exporter.getvalue(), eval_depth=EVAL_DEPTH)

        #     ingested += 1
        #     print(f"  Ingested game {ingested}", end="\r")
        while ingested < limit:
            import chess.pgn
            game = chess.pgn.read_game(f)
            if game is None:
                break

            exporter = chess.pgn.StringExporter(headers=True, variations=False)
            game.accept(exporter)
            db.ingest_pgn(str(exporter), eval_depth=EVAL_DEPTH)

            ingested += 1
            print(f"  Ingested game {ingested}", end="\r")

    print(f"\nDone. {ingested} game(s) stored.")


def main():
    db = ChessGraphDB(NEO4J_URI, NEO4J_USER, NEO4J_PASSWORD, STOCKFISH_PATH)

    try:
        #PGN file ingesttt
        ingest_games(db, pgn_path=r"CHESS\neo4jSOP\evansgambit.pgn") #ive added r before string to make it raw string. can use fpaath directly now

        #USE LATER NOW IGNORE
        # board = chess.Board()
        # board.push_san("e4")
        # board.push_san("e5")
        # board.push_san("Nf3")
        # board.push_san("Nc6")
        # board.push_san("Bb5")
        # board.push_san("a6")

        #analyse_position(db, board.fen())
        #NOT NEED FOR SOP PRESENTATION

        db.ingest_pgn("1. e4 e5 2. Nf3 Nc6 3. Bb5 a6", eval_depth=EVAL_DEPTH)  #sample pos. ruy lopez morphy def


    finally:
        db.close()


if __name__ == "__main__":
    main()
