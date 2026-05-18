import torch
import math
from model import ChessNNUE
from preprocess import fen_to_tensor

# 1. Initialize the model structure
model = ChessNNUE()

# 2. Load the trained weights
# (We load using map_location='cpu' so you can run inference without waking up the GPU)
model.load_state_dict(torch.load("nnue_weights.pth", map_location='cpu'))
model.eval() # Set to evaluation mode

def evaluate_position(fen_string):
    with torch.no_grad(): # Disable gradients for speed
        # Convert FEN to 768-array
        features = fen_to_tensor(fen_string)
        
        # Convert to tensor and add batch dimension (shape: [1, 768])
        input_tensor = torch.tensor([features], dtype=torch.float32)
        
        # Run through the neural network
        prediction = model(input_tensor).item()
        
        # Convert the (0.0 to 1.0) probability back to Centipawns
        # Inverse of our sigmoid function: eval = -400 * ln((1/p) - 1)  #actually. rep 400 by 200 if ever needed this line
        if prediction <= 0.001: prediction = 0.001
        if prediction >= 0.999: prediction = 0.999
        centipawns = -400.0 * math.log((1.0 / prediction) - 1.0) 
        
        return centipawns

# Test it out!
starting_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
score = evaluate_position(starting_pos)

print(f"FEN: {starting_pos}")
print(f"Engine Evaluation: {score:.2f} centipawns")