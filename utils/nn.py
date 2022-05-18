# ==========================================================================================================================================================
# neural network (nn) module
# purpose: classes and functions to build a scalable neural network model
# ==========================================================================================================================================================

import os
import shutil
import torch as T
import torch.nn as nn
from functools import partial

def activation_function(activation):
    
    """ function that returns ModuleDict of activation functions """
    
    return  nn.ModuleDict([
        ['relu', nn.ReLU()],
        ['sigmoid', nn.Sigmoid()],
        ['softmax', nn.Softmax(1)],
        ['tanh', nn.Tanh()],
        ['none', nn.Identity()]
    ])[activation]

class conv_2d_auto_padding(nn.Conv2d):
    
    """ class to set padding dynamically based on kernel size to preserve dimensions of height and width after conv """
    
    def __init__(self, *args, **kwargs):
        
        """ class constructor for conv_2d_auto_padding to alter padding attributes of nn.Conv2d """
        
        # inherit class constructor attributes from nn.Conv2d
        super().__init__(*args, **kwargs)
        
        # dynamically adds padding based on the kernel_size
        self.padding =  (self.kernel_size[0] // 2, self.kernel_size[1] // 2) 

class fc_block(nn.Module):
    
    """ class to build basic fully connected block """
    
    def __init__(self, input_shape, output_shape, activation_func, dropout_p):
        
        """ class constructor that creates the layers attributes for fc_block """
        
        # inherit class constructor attributes from nn.Module
        super().__init__()
        
        # input and output units for hidden layer 
        self.input_shape = input_shape
        self.output_shape = output_shape
        
        # activation function for after batch norm
        self.activation_func = activation_func 
        
        # dropout probablity
        self.dropout_p = dropout_p
        
        # basic fc_block. inpuit --> linear --> batch norm --> activation function --> dropout 
        self.block = nn.Sequential(
            
            # linear hidden layer
            nn.Linear(self.input_shape, self.output_shape, bias = False),
            
            # batch norm
            nn.BatchNorm1d(self.output_shape),
            
            # activation func
            activation_function(self.activation_func),
            
            # dropout
            nn.Dropout(self.dropout_p),
            
        )
    
    def forward(self, x):
        
        """ function for forward pass of fc_block """
        
        x = self.block(x)
        
        return x

class nn_layers(nn.Module):
    
    """ class to build layers of blocks (e.g. fc_block) """
    
    def __init__(self, input_channels, block, output_channels, *args, **kwargs):
        
        # inherit class constructor attributes from nn.Module
        super().__init__()
        
        # input channels/shape
        self.input_channels = input_channels
        
        # class of block
        self.block = block
        
        # output channels/shape
        self.output_channels = output_channels
        self.input_output_list = list(zip(output_channels[:], output_channels[1:]))
        
        # module list of layers with same args and kwargs
        self.blocks = nn.ModuleList([
            
            self.block(self.input_channels, self.output_channels[0], *args, **kwargs),
            *[self.block(input_channels, output_channels, *args, **kwargs) for (input_channels, output_channels) in self.input_output_list]   
            
        ])
    
    def get_flat_output_shape(self, input_shape):
        
        """ function to obatain number of features after flattening after convolution layers """
        
        # assert that this function must be utilised on a convulution block
        assert hasattr(self.block, 'conv') == True, "Cannot execute get_flat_output_shape on non-convulution block"

        # initialise dummy tensor of ones with input shape
        x = T.ones(1, *input_shape)
        
        # feed dummy tensor to blocks by iterating over each block
        for block in self.blocks:
            
            x = block(x)
        
        # flatten resulting tensor and obtain features after flattening
        n_size = x.view(1, -1).size(1)
        
        return n_size

    def forward(self, x, *args, **kwargs):
        
        """ function for forward pass of layers """
        
        # iterate over each block
        for block in self.blocks:
            
            x = block(x, *args, **kwargs)
            
        return x 

class maddpgv2_mlp_actor_model(nn.Module):
    
    """ class to build model for MADDPGv2 """
    
    def __init__(self, model, model_name, mode, scenario_name, training_name, learning_rate, dropout_p, fc_input_dims, fc_output_dims, tanh_actions_dims):
        
        """ class constructor for attributes for the actor model """
        
        # inherit class constructor attributes from nn.Module
        super().__init__()
        
        # model
        self.model = model
        
        # model name
        self.model_name = model_name
        
        # checkpoint filepath 
        self.checkpoint_path = None
        
        # if training model
        if mode != 'test' and mode != 'load_and_train':

            try:
                
                # create directory for saving models if it does not exist
                os.mkdir("saved_models/" + scenario_name + "/" + training_name + "_" + "best_models/")
                
            except:
                
                # remove existing directory and create new directory
                shutil.rmtree("saved_models/" + scenario_name + "/" + training_name + "_" + "best_models/")
                os.mkdir("saved_models/" + scenario_name + "/" + training_name + "_" + "best_models/")

        # checkpoint directory
        self.checkpoint_dir = "saved_models/" + scenario_name + "/" + training_name + "_" + "best_models/"
        
        # learning rate
        self.learning_rate = learning_rate

        # model architecture for maddpgv2 actor

        # hidden fc layers for obs inputs
        # input channels are the dimensions of observation from agent concatenated with the goal of the agent
        # fc_output_dims is the list of sizes of output channels fc_block
        self.actor_fc_layers = nn_layers(input_channels = fc_input_dims, block = fc_block, output_channels = fc_output_dims, activation_func = 'relu', dropout_p = dropout_p)

        # final fc_blocks for actions with tanh activation function
        self.tanh_actions_layer = fc_block(input_shape = fc_output_dims[-1], output_shape = tanh_actions_dims, activation_func = "tanh", dropout_p = dropout_p)
             
        # adam optimizer 
        self.optimizer = T.optim.Adam(self.parameters(), lr = self.learning_rate)
        
        # device for training (cpu/gpu)
        self.device = T.device('cuda:0' if T.cuda.is_available() else 'cpu')
        
        # cast module to device
        self.to(self.device)
    
    def forward(self, x):
            
        """ function for forward pass through actor model """
            
        # x (obs || goal) --> actor_fc_layers
        x = self.actor_fc_layers(x)

        # actor_fc_layers --> tanh_actions_layer
        tanh_actions = self.tanh_actions_layer(x)
        
        return tanh_actions

class maddpgv2_gnn_critic_model(nn.Module):
    
    """ class to build model for MADDPGv2 """
    
    def __init__(self, model, model_name, mode, scenario_name, training_name, learning_rate, dropout_p, fc_input_dims, fc_output_dims):
        
        """ class constructor for attributes for the model """
        
        # inherit class constructor attributes from nn.Module
        super().__init__()
        
        # model
        self.model = model
        
        # model name
        self.model_name = model_name
        
        # checkpoint filepath 
        self.checkpoint_path = None
        
        # if training model
        if mode != 'test' and mode != 'load_and_train':

            try:
                                # create directory for saving models if it does not exist
                os.mkdir("saved_models/" + scenario_name + "/" + training_name + "_" + "best_models/")
                
            except:
                
                # remove existing directory and create new directory
                shutil.rmtree("saved_models/" + scenario_name + "/" + training_name + "_" + "best_models/")
                os.mkdir("saved_models/" + scenario_name + "/" + training_name + "_" + "best_models/")

        # checkpoint directory
        self.checkpoint_dir = "saved_models/" + scenario_name + "/" + training_name + "_" + "best_models/"
        
        # learning rate
        self.learning_rate = learning_rate
            
        # model architecture for maddpgv2 critic

        # hidden fc layers for obs inputs
        # input channels are the dimensions of observation from agent concatenated with the goal of the agent
        # fc_output_dims is the list of sizes of output channels fc_block
        self.critic_fc_layers = nn_layers(input_channels = fc_input_dims, block = fc_block, output_channels = fc_output_dims, activation_func = 'relu', dropout_p = dropout_p)

        # final fc_block for Q value output w/o activation function
        self.q_layer = fc_block(input_shape = fc_output_dims[-1], output_shape = 1, activation_func = "none", dropout_p = dropout_p)
            
        # adam optimizer 
        self.optimizer = T.optim.Adam(self.parameters(), lr = self.learning_rate)
        
        # device for training (cpu/gpu)
        self.device = T.device('cuda:0' if T.cuda.is_available() else 'cpu')
        
        # cast module to device
        self.to(self.device)
    
    def forward(self, state, action, goal):
            
        """ function for forward pass through critic model """
        
        # conc = states || actions || goals
        conc = T.cat((state, action, goal), 1)

        # conc --> critic_concat_fc_layers
        conc = self.critic_fc_layers(x = conc)

        # critic_concat_fc_layers --> q value
        q = self.q_layer(conc)
        
        return q