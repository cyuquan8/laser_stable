# ==========================================================================================================================================================
# neural network (nn) module
# purpose: classes and functions to build a scalable neural network model
# ==========================================================================================================================================================

import os
import shutil
import math
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
        ['hard_tanh', nn.Hardtanh()],
        ['none', nn.Identity()]
    ])[activation]

def weights_initialisation_function_generator(weight_intialisation, activation_func, *args, **kwargs):

    """ function that returns functions initialise weights according to specified methods """
        
    # check weight initialisation
    if weight_intialisation == "xavier_uniform":
        
        # generate function
        def init_weight(m):
            
            # check if linear
            if isinstance(m, nn.Linear):

                # initialise weight
                nn.init.xavier_uniform_(m.weight, gain = nn.init.calculate_gain(activation_func))
        
        return init_weight

    # check weight initialisation
    elif weight_intialisation == "xavier_normal":
        
        # generate function
        def init_weight(m):
            
            # check if linear
            if isinstance(m, nn.Linear):

                # initialise weight
                nn.init.xavier_normal_(m.weight, gain = nn.init.calculate_gain(activation_func))
        
        return init_weight
    
    # check weight initialisation
    elif weight_intialisation == "kaiming_uniform":
        
        # recommend for relu / leaky relu
        assert (activation_func == "relu" or activation_func == "leaky_relu"), "Non-linearity recommended to be 'relu' or 'leaky_relu'"

        # generate function
        def init_weight(m):
            
            # check if linear
            if isinstance(m, nn.Linear):

                # initialise weight
                nn.init.kaiming_uniform_(m.weight, a = kwargs.get("kaiming_a", math.sqrt(5)), mode = kwargs.get("kaiming_mode", "fan_in"), nonlinearity = activation_func)
        
        return init_weight
    
    # check weight initialisation
    elif weight_intialisation == "kaiming_normal":
        
        # recommend for relu / leaky relu
        assert (activation_func == "relu" or activation_func == "leaky_relu"), "Non-linearity recommended to be 'relu' or 'leaky_relu'"

        # generate function
        def init_weight(m):
            
            # check if linear
            if isinstance(m, nn.Linear):

                # initialise weight
                nn.init.kaiming_normal_(m.weight, a = kwargs.get("kaiming_a", math.sqrt(5)), mode = kwargs.get("kaiming_mode", "fan_in"), nonlinearity = activation_func)

        return init_weight

    # check weight initialisation
    elif weight_intialisation == "uniform":

        # generate function
        def init_weight(m):
            
            # check if linear
            if isinstance(m, nn.Linear):

                # initialise weight
                nn.init.uniform_(m.weight, a = kwargs.get("uniform_lower_bound", 0.0), b = kwargs.get("uniform_upper_bound", 1.0))

        return init_weight

    else:
        
        # generate function
        def init_weight(m):

            pass

        return init_weight

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
    
    def __init__(self, input_shape, output_shape, activation_func, dropout_p, weight_initialisation, *args, **kwargs):
        
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
        
        # weight initialisation
        self.block.apply(weights_initialisation_function_generator(weight_initialisation, activation_func, *args, **kwargs))

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
    
    def __init__(self, model, model_name, mode, scenario_name, training_name, learning_rate, optimizer, lr_scheduler, dropout_p, fc_input_dims, fc_output_dims, tanh_actions_dims, *args, **kwargs):
        
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
        self.actor_fc_layers = nn_layers(input_channels = fc_input_dims, block = fc_block, output_channels = fc_output_dims, activation_func = 'relu', dropout_p = dropout_p, 
                                         weight_initialisation = "kaiming_uniform")

        # final fc_blocks for actions with tanh activation function
        self.tanh_actions_layer = fc_block(input_shape = fc_output_dims[-1], output_shape = tanh_actions_dims, activation_func = "tanh", dropout_p = dropout_p, 
                                           weight_initialisation = "xavier_uniform")
        
        # check optimizer
        if optimizer == "adam":

            # adam optimizer 
            self.optimizer = T.optim.Adam(self.parameters(), lr = self.learning_rate)
        
        # check learning rate scheduler
        if lr_scheduler == "cosine_annealing_with_warm_restarts":

            self.scheduler = T.optim.lr_scheduler.CosineAnnealingWarmRestarts(optimizer = self.optimizer, T_0 = kwargs.get('actor_lr_scheduler_T_0', 1000), eta_min = kwargs.get('actor_lr_scheduler_eta_min', 0))
        
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

class maddpgv2_mlp_critic_model(nn.Module):
    
    """ class to build model for MADDPGv2 """
    
    def __init__(self, model, model_name, mode, scenario_name, training_name, learning_rate, optimizer, lr_scheduler, dropout_p, fc_input_dims, fc_output_dims, *args, **kwargs):
        
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
        self.critic_fc_layers = nn_layers(input_channels = fc_input_dims, block = fc_block, output_channels = fc_output_dims, activation_func = 'relu', dropout_p = dropout_p, 
                                          weight_initialisation = "kaiming_uniform")

        # final fc_block for Q value output w/o activation function
        self.q_layer = fc_block(input_shape = fc_output_dims[-1], output_shape = 1, activation_func = "none", dropout_p = dropout_p, weight_initialisation = "default")
            
        # check optimizer
        if optimizer == "adam":

            # adam optimizer 
            self.optimizer = T.optim.Adam(self.parameters(), lr = self.learning_rate)
        
        # check learning rate scheduler
        if lr_scheduler == "cosine_annealing_with_warm_restarts":

            self.scheduler = T.optim.lr_scheduler.CosineAnnealingWarmRestarts(optimizer = self.optimizer, T_0 = kwargs.get('critic_lr_scheduler_T_0', 1000), eta_min = kwargs.get('critic_lr_scheduler_eta_min', 0))
        
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

class maddpgv2_lstm_actor_model(nn.Module):
    
    """ class to build model for MADDPGv2 """
    
    def __init__(self, model, model_name, mode, scenario_name, training_name, learning_rate, optimizer, lr_scheduler, dropout_p, lstm_sequence_length, lstm_input_size, lstm_hidden_size, lstm_num_layers, 
                 tanh_actions_dims, *args, **kwargs):
        
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
        
        # lstm parameters
        self.lstm_sequence_length = lstm_sequence_length
        self.lstm_input_size = lstm_input_size

        # model architecture for maddpgv2 actor

        # lstm layer to process sequence based inputs
        # lstm_input_size is the number of expected features in the input x
        # lstm_hidden_size is the number of features in the hidden state h
        # lstm_num_layers is the number of recurrent layers
        self.actor_lstm = T.nn.LSTM(input_size = lstm_input_size, hidden_size = lstm_hidden_size, num_layers = lstm_num_layers, bias = True, batch_first = True, dropout = dropout_p, 
                                    bidirectional = kwargs.get("actor_lstm_bidirectional", False), proj_size = 0)

        # final fc_blocks for actions with no activation function
        self.actions_layer = fc_block(input_shape = lstm_sequence_length * lstm_input_size * 2 if kwargs.get("actor_lstm_bidirectional", False) == True else lstm_sequence_length * lstm_input_size, 
                                      output_shape = tanh_actions_dims, activation_func = "none", dropout_p = dropout_p, weight_initialisation = "default")
        
        # check optimizer
        if optimizer == "adam":

            # adam optimizer 
            self.optimizer = T.optim.Adam(self.parameters(), lr = self.learning_rate)
        
        # check learning rate scheduler
        if lr_scheduler == "cosine_annealing_with_warm_restarts":

            self.scheduler = T.optim.lr_scheduler.CosineAnnealingWarmRestarts(optimizer = self.optimizer, T_0 = kwargs.get('actor_lr_scheduler_T_0', 1000), eta_min = kwargs.get('actor_lr_scheduler_eta_min', 0))
        
        # device for training (cpu/gpu)
        self.device = T.device('cuda:0' if T.cuda.is_available() else 'cpu')
        
        # cast module to device
        self.to(self.device)
    
    def forward(self, x):
            
        """ function for forward pass through actor model """

        # obtain shape of x, (obs || goal)
        size = x.size()

        # check if sequence length * input size = state dimensions
        if size[1] == self.lstm_sequence_length * self.lstm_input_size: 

            # change shape of x (obs || goal) from (batch size, state dimensions) to (batch size, sequence length, input size), where sequence length * input size = state dimensions
            x = x.view(size[0], self.lstm_sequence_length, self.lstm_input_size)

        # sequence length * input size > state dimensions
        else:
            
            # zero pad the shape of x from (batch size, state dimensions) to (batch size, sequence length * input size)
            x = T.cat((x, T.zeros(size[0], self.lstm_sequence_length * self.lstm_input_size - size[1])), 1)

            # change shape of x from (batch size, sequence length * input size) to (batch size, sequence length, input size)
            x = x.view(size[0], self.lstm_sequence_length, self.lstm_input_size)

        # x --> actor_lstm
        x = self.actor_lstm(x)
      
        # change shape of x from (batch size, sequence length, hidden_size) to (batch_size, sequence length * hidden_size)
        # ignore h_n, c_n
        x = x[0].reshape(size[0], -1)
       
        # actor_lstm --> actions_layer
        actions = self.actions_layer(x)
        
        return actions

class maddpgv2_lstm_critic_model(nn.Module):
    
    """ class to build model for MADDPGv2 """
    
    def __init__(self, model, model_name, mode, scenario_name, training_name, learning_rate, optimizer, lr_scheduler, dropout_p, lstm_sequence_length, lstm_input_size, lstm_hidden_size, lstm_num_layers, 
                 *args, **kwargs):
        
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
    
        # lstm parameters
        self.lstm_sequence_length = lstm_sequence_length
        self.lstm_input_size = lstm_input_size

        # model architecture for maddpgv2 critic

        # lstm layer to process sequence based inputs
        # lstm_input_size is the number of expected features in the input x
        # lstm_hidden_size is the number of features in the hidden state h
        # lstm_num_layers is the number of recurrent layers
        self.critic_lstm = T.nn.LSTM(input_size = lstm_input_size, hidden_size = lstm_hidden_size, num_layers = lstm_num_layers, bias = True, batch_first = True, dropout = dropout_p, 
                                     bidirectional = kwargs.get("critic_lstm_bidirectional", False), proj_size = 0)

        # final fc_block for Q value output w/o activation function
        self.q_layer = fc_block(input_shape = lstm_sequence_length * lstm_input_size * 2 if kwargs.get("critic_lstm_bidirectional", False) == True else lstm_sequence_length * lstm_input_size, output_shape = 1, 
                                activation_func = "none", dropout_p = dropout_p, weight_initialisation = "default")
            
        # check optimizer
        if optimizer == "adam":

            # adam optimizer 
            self.optimizer = T.optim.Adam(self.parameters(), lr = self.learning_rate)
        
        # check learning rate scheduler
        if lr_scheduler == "cosine_annealing_with_warm_restarts":

            self.scheduler = T.optim.lr_scheduler.CosineAnnealingWarmRestarts(optimizer = self.optimizer, T_0 = kwargs.get('critic_lr_scheduler_T_0', 1000), eta_min = kwargs.get('critic_lr_scheduler_eta_min', 0))
        
        # device for training (cpu/gpu)
        self.device = T.device('cuda:0' if T.cuda.is_available() else 'cpu')
        
        # cast module to device
        self.to(self.device)
    
    def forward(self, state, action, goal):
            
        """ function for forward pass through critic model """
        
        # conc = states || actions || goals
        conc = T.cat((state, action, goal), 1)

        # obtain shape of conc 
        size = conc.size()

        # check if sequence length * input size = state dimensions
        if size[1] == self.lstm_sequence_length * self.lstm_input_size: 

            # change shape of x (obs || goal) from (batch size, state dimensions) to (batch size, sequence length, input size), where sequence length * input size = state dimensions
            conc = conc.view(size[0], self.lstm_sequence_length, self.lstm_input_size)

        # sequence length * input size > state dimensions
        else:
            
            # zero pad the shape of x from (batch size, state dimensions) to (batch size, sequence length * input size)
            conc = T.cat((conc, T.zeros(size[0], self.lstm_sequence_length * self.lstm_input_size - size[1])), 1)

            # change shape of x from (batch size, sequence length * input size) to (batch size, sequence length, input size)
            conc = conc.view(size[0], self.lstm_sequence_length, self.lstm_input_size)

        # conc --> critic_concat_fc_layers
        # ignore h_n, c_n
        conc = self.critic_lstm(conc)

        # change shape of x from (batch size, sequence length, hidden_size) to (batch_size, sequence length * hidden_size)
        # ignore h_n, c_n
        conc = conc[0].reshape(size[0], -1)

        # critic_lstm --> q value
        q = self.q_layer(conc)
        
        return q