NEO4J_URI       = "neo4j://localhost:7687"
NEO4J_USER      = "neo4j"
NEO4J_PASSWORD  = "whypassword"
STOCKFISH_PATH  = r"C:\Users\Asus\Desktop\ChessTest\stockfish\stockfish-windows-x86-64-avx2.exe" #"/usr/local/bin/stockfish"

EVAL_THRESHOLD  = 1.5   # pawns — positions more favourable than this are "good"
MAX_PATH_DEPTH  = 12    # max moves to search ahead in the graph
EVAL_DEPTH      = 16    # stockfish search depth for ingestion
INGEST_LIMIT    = 50    # number of games to ingest from a PGN file
