# ==========================================================================================================================================================
# maddpgv2 lstm agent class 
# purpose: creates and updates neural network 
# ==========================================================================================================================================================

import torch as T
from utils.nn import maddpgv2_lstm_actor_model, maddpgv2_lstm_critic_model

class maddpgv2_lstm_agent:
    
    def __init__(self, mode, scenario_name, training_name, discount_rate, lr_actor, lr_critic, optimizer, actor_lr_scheduler, critic_lr_scheduler, num_agents, actor_dropout_p, critic_dropout_p, 
                 actor_lstm_sequence_length, actor_lstm_input_size, actor_lstm_hidden_size, actor_lstm_num_layers, action_dims, critic_lstm_sequence_length, critic_lstm_input_size, critic_lstm_hidden_size, 
                 critic_lstm_num_layers, tau, actor_action_noise, actor_action_range, *args, **kwargs):
        
        """ class constructor for maddpg agent attributes """
          
        # discount rate for critic loss (TD error)
        self.discount_rate = discount_rate
        
        # learning rate for actor model
        self.lr_actor = lr_actor
        
        # learning rate for critic model
        self.lr_critic = lr_critic
        
        # number of agents
        self.num_agents = num_agents

        # softcopy parameter for target network 
        self.tau = tau
        
        # actor_action_noise (constant multiplied to N(0, 1) gaussian)
        self.actor_action_noise = actor_action_noise

        # range of action
        self.actor_action_range = actor_action_range

        # intialise actor model 
        self.maddpgv2_lstm_actor = maddpgv2_lstm_actor_model(model = "maddpgv2_lstm_actor", model_name = None, mode = mode, scenario_name = scenario_name, training_name = training_name, 
                                                             learning_rate = self.lr_actor, optimizer = optimizer, lr_scheduler = actor_lr_scheduler, dropout_p = actor_dropout_p, 
                                                             lstm_sequence_length = actor_lstm_sequence_length, lstm_input_size = actor_lstm_input_size, lstm_hidden_size = actor_lstm_hidden_size, 
                                                             lstm_num_layers = actor_lstm_num_layers, tanh_actions_dims = action_dims, *args, **kwargs)
                         
        # intialise target actor model
        self.maddpgv2_lstm_target_actor = maddpgv2_lstm_actor_model(model = "maddpgv2_lstm_actor", model_name = None, mode = mode, scenario_name = scenario_name, training_name = training_name, 
                                                                    learning_rate = self.lr_actor, optimizer = optimizer, lr_scheduler = actor_lr_scheduler, dropout_p = actor_dropout_p, 
                                                                    lstm_sequence_length = actor_lstm_sequence_length, lstm_input_size = actor_lstm_input_size, lstm_hidden_size = actor_lstm_hidden_size, 
                                                                    lstm_num_layers = actor_lstm_num_layers, tanh_actions_dims = action_dims, *args, **kwargs)

        # intialise critic model
        self.maddpgv2_lstm_critic = maddpgv2_lstm_critic_model(model = "maddpgv2_lstm_critic", model_name = None, mode = mode, scenario_name = scenario_name, training_name = training_name, 
                                                               learning_rate = self.lr_critic, optimizer = optimizer, lr_scheduler = critic_lr_scheduler, dropout_p = critic_dropout_p, 
                                                               lstm_sequence_length = critic_lstm_sequence_length, lstm_input_size = critic_lstm_input_size, lstm_hidden_size = critic_lstm_hidden_size, 
                                                               lstm_num_layers = critic_lstm_num_layers, *args, **kwargs)

        # intialise target critic model
        self.maddpgv2_lstm_target_critic = maddpgv2_lstm_critic_model(model = "maddpgv2_lstm_critic", model_name = None, mode = mode, scenario_name = scenario_name, training_name = training_name, 
                                                                      learning_rate = self.lr_critic, optimizer = optimizer, lr_scheduler = critic_lr_scheduler, dropout_p = critic_dropout_p, 
                                                                      lstm_sequence_length = critic_lstm_sequence_length, lstm_input_size = critic_lstm_input_size, lstm_hidden_size = critic_lstm_hidden_size, 
                                                                      lstm_num_layers = critic_lstm_num_layers, *args, **kwargs)

        # hard update target models' weights to online network to match initialised weights
        self.update_maddpgv2_lstm_target_models(tau = 1)
        
    def update_maddpgv2_lstm_target_models(self, tau = None): 
        
        """ function to soft update target model weights for maddpg. hard update is possible if tau = 1 """
        
        # use tau attribute if tau not specified 
        if tau is None:
            
            tau = self.tau
        
        # iterate over coupled target actor and actor parameters 
        for target_actor_parameters, actor_parameters in zip(self.maddpgv2_lstm_target_actor.parameters(), self.maddpgv2_lstm_actor.parameters()):
            
            # apply soft update to target actor
            target_actor_parameters.data.copy_((1 - tau) * target_actor_parameters.data + tau * actor_parameters.data)
        
        # iterate over coupled target critic and critic parameters
        for target_critic_parameters, critic_parameters in zip(self.maddpgv2_lstm_target_critic.parameters(), self.maddpgv2_lstm_critic.parameters()):

            # apply soft update to target critic
            target_critic_parameters.data.copy_((1 - tau) * target_critic_parameters.data + tau * critic_parameters.data)
    
    def select_action(self, mode, state):
        
        """ function to select action for the agent given state observed by local agent """
        
        # set actor model to evaluation mode (for batch norm and dropout) --> remove instances of batch norm, dropout etc. (things that shd only be around in training)
        self.maddpgv2_lstm_actor.eval()
        
        # turn actor local state observations to tensor in actor device
        actor_input = T.tensor(state, dtype = T.float).to(self.maddpgv2_lstm_actor.device)
        
        # add batch dimension to inputs
        actor_input = actor_input.unsqueeze(0)
    
        # feed actor_input to obtain motor and communication actions 
        action = self.maddpgv2_lstm_actor.forward(actor_input)

        # add gaussian noise if not test
        if mode != "test":

            # generate gaussian noise for motor and communication actions
            action_noise = T.mul(T.normal(mean = 0.0, std = 1, size = action.size()), self.actor_action_noise).to(self.maddpgv2_lstm_actor.device)

            # add to noise to motor and communication actions 
            action = T.add(action, action_noise)
        
        # multiply action by actor_action_range
        action = T.mul(action, self.actor_action_range)

        # # clamp action by actor_action_range
        # action = T.clamp(action, - self.actor_action_range, self.actor_action_range)

        # set actor model to training mode (for batch norm and dropout)
        self.maddpgv2_lstm_actor.train()
         
        return action.cpu().detach().numpy()[0]
    
    def save_models(self):
        
        """ save weights """
        
        # save weights for each actor, target_actor, critic, target_critic model
        T.save(self.maddpgv2_lstm_actor.state_dict(), self.maddpgv2_lstm_actor.checkpoint_path)
        T.save(self.maddpgv2_lstm_target_actor.state_dict(), self.maddpgv2_lstm_target_actor.checkpoint_path)
        T.save(self.maddpgv2_lstm_critic.state_dict(), self.maddpgv2_lstm_critic.checkpoint_path)
        T.save(self.maddpgv2_lstm_target_critic.state_dict(), self.maddpgv2_lstm_target_critic.checkpoint_path)
        
    def load_models(self):
        
        """ load weights """
        
        # load weights for each actor, target_actor, critic, target_critic model
        self.maddpgv2_lstm_actor.load_state_dict(T.load(self.maddpgv2_lstm_actor.checkpoint_path, map_location = T.device('cuda:0' if T.cuda.is_available() else 'cpu')))
        self.maddpgv2_lstm_target_actor.load_state_dict(T.load(self.maddpgv2_lstm_target_actor.checkpoint_path, map_location = T.device('cuda:0' if T.cuda.is_available() else 'cpu')))
        self.maddpgv2_lstm_critic.load_state_dict(T.load(self.maddpgv2_lstm_critic.checkpoint_path, map_location = T.device('cuda:0' if T.cuda.is_available() else 'cpu')))
        self.maddpgv2_lstm_target_critic.load_state_dict(T.load(self.maddpgv2_lstm_target_critic.checkpoint_path, map_location = T.device('cuda:0' if T.cuda.is_available() else 'cpu')))