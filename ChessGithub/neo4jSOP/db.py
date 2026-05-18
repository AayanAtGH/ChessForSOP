import io
import chess
import chess.pgn
import chess.engine
from neo4j import GraphDatabase

from config import EVAL_THRESHOLD, MAX_PATH_DEPTH


class ChessGraphDB:
    def __init__(self, uri: str, user: str, password: str, stockfish_path: str):
        self.driver = GraphDatabase.driver(uri, auth=(user, password))
        self.engine = chess.engine.SimpleEngine.popen_uci(stockfish_path)
        self._create_constraints()

    def close(self):
        self.driver.close()
        self.engine.quit()

    # ------------------------------------------------------------------
    # Schema
    # ------------------------------------------------------------------

    def _create_constraints(self):
        """Unique constraint on FEN so transpositions share one node."""
        with self.driver.session() as session:
            session.run("""
                CREATE CONSTRAINT position_fen IF NOT EXISTS
                FOR (p:Position) REQUIRE p.fen IS UNIQUE
            """)
            session.run("""
                CREATE INDEX position_eval IF NOT EXISTS
                FOR (p:Position) ON (p.eval)
            """)

    # ------------------------------------------------------------------
    # Evaluation
    # ------------------------------------------------------------------

    def _evaluate(self, board: chess.Board, depth: int = 18) -> float:
        """Return eval in pawns from White's perspective."""
        result = self.engine.analyse(board, chess.engine.Limit(depth=depth))
        score = result["score"].white()
        if score.is_mate():
            return 999.0 if score.mate() > 0 else -999.0
        return score.score() / 100.0

    # ------------------------------------------------------------------
    # Fingerprint (structural similarity without clock/en-passant noise)
    # ------------------------------------------------------------------

    @staticmethod
    def _fen_fingerprint(fen: str) -> str:
        """
        Keep piece placement, side to move, and castling rights only.
        Two positions with the same pieces and squares share a fingerprint
        even if reached via different move orders.
        """
        parts = fen.split()
        return " ".join(parts[:3])

    # ------------------------------------------------------------------
    # Ingestion
    # ------------------------------------------------------------------

    def ingest_pgn(self, pgn_text: str, eval_depth: int = 16):
        """Walk every move in a PGN, evaluate each position, write to Neo4j."""
        game = chess.pgn.read_game(io.StringIO(pgn_text))
        if game is None:
            return

        board = game.board()

        prev_fen = board.fen()
        prev_eval = self._evaluate(board, eval_depth)
        self._upsert_position(prev_fen, prev_eval)

        for move in game.mainline_moves():
            san = board.san(move)
            uci = move.uci()
            move_number = board.fullmove_number

            board.push(move)
            curr_fen = board.fen()
            curr_eval = self._evaluate(board, eval_depth)

            self._upsert_position(curr_fen, curr_eval)
            self._upsert_move(prev_fen, curr_fen, san, uci, move_number)

            prev_fen = curr_fen

    def _upsert_position(self, fen: str, eval_score: float):
        fingerprint = self._fen_fingerprint(fen)
        side = "white" if " w " in fen else "black"
        with self.driver.session() as session:
            session.run("""
                MERGE (p:Position {fen: $fen})
                ON CREATE SET
                    p.eval         = $eval,
                    p.side_to_move = $side,
                    p.fingerprint  = $fp
                ON MATCH SET
                    p.eval = CASE
                        WHEN abs($eval) > abs(p.eval) THEN $eval
                        ELSE p.eval
                    END,
                    p.fingerprint = $fp
            """, fen=fen, eval=eval_score, side=side, fp=fingerprint)

    def _upsert_move(
        self,
        from_fen: str,
        to_fen: str,
        san: str,
        uci: str,
        move_number: int,
    ):
        with self.driver.session() as session:
            session.run("""
                MATCH (a:Position {fen: $from_fen})
                MATCH (b:Position {fen: $to_fen})
                MERGE (a)-[r:MOVE {uci: $uci}]->(b)
                ON CREATE SET r.san = $san, r.move_number = $move_number
            """, from_fen=from_fen, to_fen=to_fen,
                 san=san, uci=uci, move_number=move_number)

    # ------------------------------------------------------------------
    # Queries
    # ------------------------------------------------------------------

    def find_lines_to_favourable(
        self,
        current_fen: str,
        eval_threshold: float = EVAL_THRESHOLD,
        max_depth: int = MAX_PATH_DEPTH,
        limit: int = 5,
    ) -> list[dict]:
        """
        From `current_fen`, find graph paths to positions where eval
        exceeds the threshold (in the current side's favour).
        Returns each path as an ordered move list plus the target eval.
        """
        side = "white" if " w " in current_fen else "black"
        eval_condition = (
            "target.eval >= $threshold"
            if side == "white"
            else "target.eval <= -$threshold"
        )

        query = f"""
            MATCH path = (start:Position {{fen: $fen}})
                         -[:MOVE*1..{max_depth}]->
                         (target:Position)
            WHERE {eval_condition}
              AND all(r IN relationships(path) WHERE r.move_number IS NOT NULL)
            WITH path, target,
                 [r IN relationships(path) | r.san] AS moves,
                 length(path)                        AS depth
            RETURN moves,
                   target.fen  AS target_fen,
                   target.eval AS target_eval,
                   depth
            ORDER BY abs(target.eval) DESC, depth ASC
            LIMIT $limit
        """

        with self.driver.session() as session:
            results = session.run(
                query,
                fen=current_fen,
                threshold=abs(eval_threshold),
                limit=limit,
            )
            return [dict(r) for r in results]

    def find_similar_favourable(
        self,
        current_fen: str,
        eval_threshold: float = EVAL_THRESHOLD,
    ) -> list[dict]:
        """
        Find stored positions that share piece placement and castling rights
        with `current_fen` and have a favourable eval.
        Ranked by absolute eval descending.
        """
        fp = self._fen_fingerprint(current_fen)
        side = "white" if " w " in current_fen else "black"
        eval_cond = (
            "p.eval >= $thr" if side == "white" else "p.eval <= -$thr"
        )

        query = f"""
            MATCH (p:Position)
            WHERE p.fingerprint = $fp
              AND {eval_cond}
            RETURN p.fen AS fen, p.eval AS eval
            ORDER BY abs(p.eval) DESC
            LIMIT 10
        """

        with self.driver.session() as session:
            return [
                dict(r)
                for r in session.run(query, fp=fp, thr=abs(eval_threshold))
            ]
