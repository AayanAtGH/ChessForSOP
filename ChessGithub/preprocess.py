#parses csv file to make data chunks for pytorch

import csv
import torch
import os
import math

# Map pieces to their bitboard index (0-11)
PIECE_MAP = {'P':0, 'N':1, 'B':2, 'R':3, 'Q':4, 'K':5, 
             'p':6, 'n':7, 'b':8, 'r':9, 'q':10, 'k':11}

# def fen_to_tensor(fen):
#     """Parses a FEN string into a 768-element feature array."""
#     features = [0.0] * 768
#     board_part = fen.split(' ')[0] # Get only the piece placement
    
#     #square = 0
#     fen_square = 0

#     for char in board_part:
#         if char == '/':
#             continue
#         if char.isdigit():
#             #square += int(char) # Skip empty squares
#             fen_square += int(char)
#         else:
#             # # Calculate exact index: piece_type * 64 + square
#             # piece_idx = PIECE_MAP[char]
#             # feature_idx = piece_idx * 64 + square
#             # features[feature_idx] = 1.0
#             # square += 1

#             piece_idx = PIECE_MAP[char]
            
#             # Convert FEN square (a8=0) to engine square (a1=0)
#             rank = 7 - (fen_square // 8)
#             file = fen_square % 8
#             engine_square = rank * 8 + file
            
#             feature_idx = piece_idx * 64 + engine_square
#             features[feature_idx] = 1.0
#             fen_square += 1
            
#     return features




def fen_to_tensor(fen):
    """
    768 piece features
    +  1 side to move
    +  4 castling rights (wK, wQ, bK, bQ)
    +  8 en passant file (one-hot a-h, all zero if none)
    = 781 total features     #ADDED NEURONS FOR SIDE TO MOVE, CASTLING, ENPASSANT
    """
    parts = fen.split(' ')
    board_part     = parts[0]
    side_to_move   = parts[1]          # 'w' or 'b'
    castling       = parts[2]          # e.g. 'KQkq', '-'
    en_passant     = parts[3]          # e.g. 'e3', '-'

    features = [0.0] * 781

    # --- 768 piece features ---
    fen_square = 0
    for char in board_part:
        if char == '/':
            continue
        if char.isdigit():
            fen_square += int(char)
        else:
            piece_idx = PIECE_MAP[char]

            # rank = 7 - (fen_square // 8)    #AWWWW HELLLL NAWWWWW
            # file = fen_square % 8
            # engine_square = rank * 8 + file
            engine_square = fen_square  #NO CONVERSION NEEDED, BBC, FEN BOTH USE a8=0


            features[piece_idx * 64 + engine_square] = 1.0
            fen_square += 1

    # --- index 768: side to move ---
    features[768] = 1.0 if side_to_move == 'w' else 0.0

    # --- indices 769-772: castling rights ---
    features[769] = 1.0 if 'K' in castling else 0.0  # white kingside
    features[770] = 1.0 if 'Q' in castling else 0.0  # white queenside
    features[771] = 1.0 if 'k' in castling else 0.0  # black kingside
    features[772] = 1.0 if 'q' in castling else 0.0  # black queenside

    # --- indices 773-780: en passant file (one-hot) ---
    if en_passant != '-':
        ep_file = ord(en_passant[0]) - ord('a')  # 'a'=0 ... 'h'=7
        features[773 + ep_file] = 1.0

    return features






def parse_evaluation(eval_str):
    """Safely converts evaluation strings to centipawns, handling mate scores."""
    eval_str = eval_str.strip()
    
    # Handle Forced Mates
    if eval_str.startswith('#+'):
        return 10000.0  # Absolute win for White
    elif eval_str.startswith('#-'):
        return -10000.0 # Absolute win for Black
    
    # Handle standard centipawns
    try:
        return float(eval_str)
    except ValueError:
        # Fallback if there's corrupted data in the CSV
        return 0.0

def normalize_evaluation(eval_centipawns):
    """Maps an evaluation to a Win Probability (0.0 to 1.0) to prevent exploding gradients."""
    # Cap evaluations to prevent math overflow errors on crazy engine scores
    eval_centipawns = max(-10000.0, min(10000.0, eval_centipawns))
    return 1.0 / (1.0 + math.exp(-eval_centipawns / 400.0)) 


def process_csv_in_chunks(csv_path, chunk_size=500000):
    os.makedirs("data_chunks", exist_ok=True)
    
    current_features = []
    current_evals = []
    chunk_index = 0
    count = 0
    
    print(f"Starting to parse {csv_path}...")
    
    with open(csv_path, 'r', encoding='utf-8') as file:
        reader = csv.reader(file)
        next(reader, None) # Safely skip header if it exists
        
        for row in reader:
            # Ensure the row has at least 2 columns to prevent index errors
            if len(row) < 2:
                continue 
                
            fen = row[0]

            # Use our new safe parser
            raw_eval = parse_evaluation(row[1]) 
            
            current_features.append(fen_to_tensor(fen))
            current_evals.append([normalize_evaluation(raw_eval)])
            count += 1
            
            if count % chunk_size == 0:
                x_tensor = torch.tensor(current_features, dtype=torch.float32)
                y_tensor = torch.tensor(current_evals, dtype=torch.float32)
                
                torch.save((x_tensor, y_tensor), f"data_chunks/chunk_{chunk_index}.pt")
                print(f"Saved chunk_{chunk_index}.pt ({count} positions processed)")
                
                current_features = []
                current_evals = []
                chunk_index += 1
                
    # Save whatever is left over
    if current_features:
        x_tensor = torch.tensor(current_features, dtype=torch.float32)
        y_tensor = torch.tensor(current_evals, dtype=torch.float32)
        torch.save((x_tensor, y_tensor), f"data_chunks/chunk_{chunk_index}.pt")
        print(f"Saved final chunk_{chunk_index}.pt. Preprocessing complete!")

if __name__ == "__main__":
    # Fixed the path string formatting here (using forward slash)
    process_csv_in_chunks("CHESS/chessData.csv", chunk_size=500000)