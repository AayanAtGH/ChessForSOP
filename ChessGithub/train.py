import random

import torch
import torch.nn as nn
import torch.optim as optim
import intel_extension_for_pytorch as ipex
import os
import glob
from model import ChessNNUE

# --- Hardware Setup ---
device = torch.device("xpu")
print(f"Training on: {torch.xpu.get_device_name(0)}")

# Initialize Model
model = ChessNNUE().to(device)
#model = ChessNNUE(training_mode=True).to(device)

#criterion = nn.MSELoss()

# train_.py — FIX       #CLAUDE : CHANGED MSE LOSS TO BCE LOSS
criterion = nn.BCELoss()
# BCELoss = -[y*log(p) + (1-y)*log(1-p)], correct for sigmoid outputs

#optimizer = optim.Adam(model.parameters(), lr=0.001)
optimizer = optim.AdamW(model.parameters(), lr=0.001, weight_decay=1e-4)    #Adamw now



# Optimize for Intel Arc
model, optimizer = ipex.optimize(model, optimizer=optimizer)



epochs = 10  #if were to change thrn also change a few lines beelow (inside TRAINING LOOP comment)

# train_.py — add after optimizer init      #claude fix
# scheduler = optim.lr_scheduler.StepLR(optimizer, step_size=5, gamma=0.5)
scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs, eta_min=1e-5)



# --- Training Loop ---
epochs = 10    #if were to change this, then also change above a few lines
batch_size = 16384 # Massive batch size for tabular NNUE data
chunk_files = glob.glob("data_chunks/chunk_*.pt")

print(f"Found {len(chunk_files)} data chunks. Starting training...\n")

# for epoch in range(epochs):
#     epoch_loss = 0.0
#     batches_processed = 0
    
#     # Iterate through the preprocessed files
#     for chunk_path in chunk_files:
#         # Load chunk into CPU memory
#         x_chunk, y_chunk = torch.load(chunk_path)
#         dataset_size = len(x_chunk)
        
#         # Mini-batching within the chunk
#         for i in range(0, dataset_size, batch_size):
#             # Slice batch and move to Intel GPU
#             inputs = x_chunk[i:i+batch_size].to(device)
#             targets = y_chunk[i:i+batch_size].to(device)
            
#             optimizer.zero_grad()
#             outputs = model(inputs)
#             loss = criterion(outputs, targets)
#             loss.backward()
#             optimizer.step()
            
#             epoch_loss += loss.item()
#             batches_processed += 1
            
#     # inside epoch loop, after all chunks:
#     scheduler.step()
#     print(f"  LR now: {scheduler.get_last_lr()[0]:.6f}")

#     avg_loss = epoch_loss / batches_processed
#     print(f"Epoch [{epoch+1}/{epochs}] completed. Average Loss: {avg_loss:.5f}")

for epoch in range(epochs):
    epoch_loss = 0.0
    batches_processed = 0

    # Shuffle chunk order each epoch
    random.shuffle(chunk_files)

    for chunk_path in chunk_files:
        x_chunk, y_chunk = torch.load(chunk_path)
        dataset_size = len(x_chunk)

        # Shuffle within the chunk
        perm = torch.randperm(dataset_size)
        x_chunk = x_chunk[perm]
        y_chunk = y_chunk[perm]

        for i in range(0, dataset_size, batch_size):
            inputs  = x_chunk[i:i+batch_size].to(device)
            targets = y_chunk[i:i+batch_size].to(device)

            optimizer.zero_grad()
            outputs = model(inputs)
            loss = criterion(outputs, targets)
            loss.backward()
            optimizer.step()

            epoch_loss += loss.item()
            batches_processed += 1

    scheduler.step()
    avg_loss = epoch_loss / batches_processed
    print(f"Epoch [{epoch+1}/{epochs}] Loss: {avg_loss:.5f}  LR: {scheduler.get_last_lr()[0]:.6f}")
    

# Save the final weights
torch.save(model.state_dict(), "nnue_weights.pth")
print("Training finished. Weights saved to 'nnue_weights.pth'.")