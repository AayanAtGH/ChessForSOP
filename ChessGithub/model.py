import torch
import torch.nn as nn

# class ChessNNUE(nn.Module):
#     # def __init__(self):
#     #     super(ChessNNUE, self).__init__()
#     #     # Input: 12 piece types * 64 squares = 768 features
#     #     self.fc1 = nn.Linear(768, 512)
#     #     self.fc2 = nn.Linear(512, 32)
#     #     self.fc3 = nn.Linear(32, 32)
#     #     self.fc4 = nn.Linear(32, 1)

#     def __init__(self, training_mode=True):
#         super().__init__()
#         #self.training_mode = training_mode
#         self.fc1 = nn.Linear(768, 512)
#         self.fc2 = nn.Linear(512, 128)   #now 128
#         self.fc3 = nn.Linear(128, 32)    #now 128
#         self.fc4 = nn.Linear(32, 1)

#     # def forward(self, x):
#     #     # Clipped ReLU clamps values between 0.0 and 1.0
#     #     x = torch.clamp(self.fc1(x), 0.0, 1.0)
#     #     x = torch.clamp(self.fc2(x), 0.0, 1.0)
#     #     x = torch.clamp(self.fc3(x), 0.0, 1.0)

#     #     # x = self.fc4(x)
#     #     # return x

#     #     # model.py — FIX      #CLAUDE FIX
#     #     x = torch.sigmoid(self.fc4(x))
#     #     return x  # now a valid probability in (0, 1)

#     def forward(self, x):
#         if self.training_mode:
#             # ReLU during training: gradients flow freely
#             x = torch.relu(self.fc1(x))
#             x = torch.relu(self.fc2(x))
#             x = torch.relu(self.fc3(x))
#         else:
#             # Clipped ReLU only at inference (matches C++ engine)
#             x = torch.clamp(self.fc1(x), 0.0, 1.0)
#             x = torch.clamp(self.fc2(x), 0.0, 1.0)
#             x = torch.clamp(self.fc3(x), 0.0, 1.0)

#         x = torch.sigmoid(self.fc4(x)) #probability to centipawn? or the other way??
#         return x


# model.py — remove training_mode entirely
class ChessNNUE(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(781, 512)   #781 now instead of 768. due to sidetomove,castling,enpassant
        self.fc2 = nn.Linear(512, 128)
        self.fc3 = nn.Linear(128, 32)
        self.fc4 = nn.Linear(32, 1)

        # # Initialize weights small so pre-activations start inside [0,1]
        # # preventing mass clamping at epoch 1
        # nn.init.uniform_(self.fc1.weight, -0.1, 0.1)
        # nn.init.uniform_(self.fc2.weight, -0.1, 0.1)
        # nn.init.uniform_(self.fc3.weight, -0.1, 0.1)
        # nn.init.zeros_(self.fc1.bias)
        # nn.init.zeros_(self.fc2.bias)
        # nn.init.zeros_(self.fc3.bias)

    def forward(self, x):
        # x = torch.clamp(self.fc1(x), 0.0, 1.0)
        # x = torch.clamp(self.fc2(x), 0.0, 1.0)
        # x = torch.clamp(self.fc3(x), 0.0, 1.0)

        # x = torch.relu(self.fc1(x)) # ReLU always, no clamp
        # x = torch.relu(self.fc2(x))
        # x = torch.relu(self.fc3(x))

        x = torch.clamp(self.fc1(x), 0.0, 1.0)  # matches C++
        x = torch.clamp(self.fc2(x), 0.0, 1.0)  # matches C++
        x = torch.clamp(self.fc3(x), 0.0, 1.0)  # matches C++

        x = torch.sigmoid(self.fc4(x))
        return x
    
    