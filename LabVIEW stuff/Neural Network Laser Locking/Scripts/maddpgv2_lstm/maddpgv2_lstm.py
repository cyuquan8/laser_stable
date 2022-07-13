# ==========================================================================================================================================================
# maddpgv2 lstm class
# purpose: class to train multiple agents
# ==========================================================================================================================================================

import os
import numpy as np
import torch as T
import torch.nn.functional as F
from maddpgv2_lstm.maddpgv2_lstm_agent import maddpgv2_lstm_agent
from maddpgv2_lstm.maddpgv2_lstm_replay_buffer import maddpgv2_lstm_replay_buffer
from utils.utils import plot_grad_flow

class maddpgv2_lstm:
    
    def __init__(self, mode, scenario_name, training_name, discount_rate, lr_actor, lr_critic, optimizer, actor_lr_scheduler, critic_lr_scheduler, num_agents, actor_dropout_p, critic_dropout_p, state_dims, 
                 goal_dims, actor_lstm_sequence_length, actor_lstm_input_size, actor_lstm_hidden_size, actor_lstm_num_layers, action_dims, critic_lstm_sequence_length, critic_lstm_input_size, 
                 critic_lstm_hidden_size, critic_lstm_num_layers, tau, actor_action_noise, actor_action_range, mem_size, batch_size, update_target, grad_clipping, grad_norm_clip, num_of_add_goals, goal_strategy, 
                 waveform_prominence, number_of_peaks_threshold, reward_multiplier_constant, *args, **kwargs):
            
        """ class constructor for attributes of the maddpg class (for multiple agents) """
        
        # list to store maddpg agents
        self.maddpgv2_lstm_agents_list = []
        
        # number of agents
        self.num_agents = num_agents

        # dimensions of action space
        self.actions_dims = action_dims
        
        # action range
        self.action_range = actor_action_range

        # batch of memory to sample
        self.batch_size = batch_size
        
        # counter for apply gradients
        self.apply_grad_counter = 0 
        
        # step for apply_grad_counter to hardcopy weights of original to target
        self.update_target = update_target
        
        # gradient clipping
        self.grad_clipping = grad_clipping
        self.grad_norm_clip = grad_norm_clip

        # iterate over num_agents
        for i in range(num_agents):

            # append maddpg agent to list
            self.maddpgv2_lstm_agents_list.append(maddpgv2_lstm_agent(mode = mode, scenario_name = scenario_name, training_name = training_name, discount_rate = discount_rate, lr_actor = lr_actor, 
                                                                      lr_critic = lr_critic, optimizer = optimizer, actor_lr_scheduler = actor_lr_scheduler, critic_lr_scheduler = critic_lr_scheduler,
                                                                      num_agents = num_agents, actor_dropout_p = actor_dropout_p, critic_dropout_p = critic_dropout_p, 
                                                                      actor_lstm_sequence_length = actor_lstm_sequence_length[i], actor_lstm_input_size = actor_lstm_input_size[i], 
                                                                      actor_lstm_hidden_size = actor_lstm_hidden_size[i], actor_lstm_num_layers = actor_lstm_num_layers[i], action_dims = action_dims, 
                                                                      critic_lstm_sequence_length = critic_lstm_sequence_length[i], critic_lstm_input_size = critic_lstm_input_size[i], 
                                                                      critic_lstm_hidden_size = critic_lstm_hidden_size[i], critic_lstm_num_layers = critic_lstm_num_layers[i], tau = tau, 
                                                                      actor_action_noise = actor_action_noise, actor_action_range = actor_action_range, *args, **kwargs))
            
            # update actor model_names attributes for checkpoints
            self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_actor.model_name = "maddpgv2_lstm_actor"
    
            # update actor checkpoints_path attributes
            self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_actor.checkpoint_path = os.path.join(self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_actor.checkpoint_dir, 
                                                                                                 self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_actor.model_name + "_" + str(i) + ".pt")
            
            # update target actor model_names attributes for checkpoints
            self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_target_actor.model_name = "maddpgv2_lstm_target_actor"
    
            # update target actor checkpoints_path attributes
            self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_target_actor.checkpoint_path = os.path.join(self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_target_actor.checkpoint_dir, 
                                                                                                        self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_target_actor.model_name + "_" + str(i) + ".pt")
            
            # update critic model_names attributes for checkpoints
            self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_critic.model_name = "maddpgv2_lstm_critic"
    
            # update critic checkpoints_path attributes
            self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_critic.checkpoint_path = os.path.join(self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_critic.checkpoint_dir, 
                                                                                                  self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_critic.model_name + "_" + str(i) + ".pt")
            
            # update target critic model_names attributes for checkpoints
            self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_target_critic.model_name = "maddpgv2_lstm_target_critic"
    
            # update target critic checkpoints_path attributes
            self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_target_critic.checkpoint_path = os.path.join(self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_target_critic.checkpoint_dir, 
                                                                                                         self.maddpgv2_lstm_agents_list[i].maddpgv2_lstm_target_critic.model_name + "_" + str(i) + ".pt")

        # if mode is not test
        if mode == 'train':
            
            # create replay buffer
            self.replay_buffer = maddpgv2_lstm_replay_buffer(mem_size = mem_size, num_agents = num_agents, actions_dims = action_dims, actor_input_dims = state_dims, 
                                                             critic_input_dims = state_dims[i] * num_agents, goal_dims = goal_dims[i], 
                                                             num_of_add_goals = num_of_add_goals, goal_strategy = goal_strategy, waveform_prominence = waveform_prominence, 
                                                             number_of_peaks_threshold = number_of_peaks_threshold, reward_multiplier_constant = reward_multiplier_constant)
    
        # if test mode
        elif mode == 'test':
            
            # load all models
            self.load_all_models()

        elif mode == "load_and_train":

            # create replay buffer
            self.replay_buffer = maddpgv2_lstm_replay_buffer(mem_size = mem_size, num_agents = num_agents, actions_dims = action_dims, actor_input_dims = state_dims, 
                                                             critic_input_dims = state_dims[i] * num_agents, goal_dims = goal_dims[i], 
                                                             num_of_add_goals = num_of_add_goals, goal_strategy = goal_strategy, waveform_prominence = waveform_prominence, 
                                                             number_of_peaks_threshold = number_of_peaks_threshold, reward_multiplier_constant = reward_multiplier_constant)
                
            # load all models
            self.load_all_models()

    def select_actions(self, mode, actor_state_list):
       
        """ function to select actions for the all agents given state observed by respective agent """
         
        # initialise empty list to store motor, communication actions and all actions from all agents
        actions_list = []

        # iterate over num_agents
        for agent_index, agent in enumerate(self.maddpgv2_lstm_agents_list):
            
            # select action for respective agent from corresponding list of states observed by agent
            action = agent.select_action(mode = mode, state = actor_state_list[agent_index])
            
            # append actions to respective lists
            actions_list.append(action)

        return np.array(actions_list)
    
    def apply_gradients_maddpgv2_lstm(self, num_of_agents):
        
        """ function to apply gradients for maddpgv2 to learn from replay buffer """

        # for anomaly detection
        T.autograd.set_detect_anomaly(True)

        # doesnt not apply gradients if memory does not have at least batch_size number of logs
        if self.replay_buffer.org_replay_buffer.mem_counter < self.batch_size:
            
            return np.nan, np.nan, np.nan, np.nan, np.nan, np.nan
        
        # sample replay buffer
        actor_state_arr, actor_state_prime_arr, actor_action_arr, actor_goals_arr, critic_state_arr, critic_state_prime_arr, critic_goals_arr, rewards, terminal = \
        self.replay_buffer.sample_log(self.batch_size)

        # obtain device (should be same for all models)
        device = self.maddpgv2_lstm_agents_list[0].maddpgv2_lstm_actor.device
    
        # turn features to tensors for critic in device
        critic_state = T.tensor(critic_state_arr, dtype = T.float).to(device)
        critic_state_prime = T.tensor(critic_state_prime_arr, dtype = T.float).to(device)
        critic_goals = T.tensor(critic_goals_arr, dtype = T.float).to(device)
        actor_action = T.tensor(actor_action_arr, dtype = T.float).to(device)
        rewards = T.tensor(rewards, dtype = T.float).to(device)
        terminal = T.tensor(terminal, dtype = T.bool).to(device)

        # generate list to store actor and target actor actions tensor output
        curr_target_actor_actions_prime_list = []
        curr_actor_actions_list = []
        past_actor_actions_list = []
        
        # enumerate over agents
        for agent_index, agent in enumerate(self.maddpgv2_lstm_agents_list):
            
            # set all models to eval mode to calculate td_target
            agent.maddpgv2_lstm_actor.eval()
            agent.maddpgv2_lstm_critic.eval()
            agent.maddpgv2_lstm_target_actor.eval()
            agent.maddpgv2_lstm_target_critic.eval()

            # convert actor_state_prime to tensor
            actor_state_prime = T.tensor(np.concatenate((actor_state_prime_arr[agent_index], actor_goals_arr[agent_index]), axis = -1), dtype = T.float).to(device)
            
            # feed actor_state_prime tensor to target actor to obtain actions
            curr_target_actor_actions_prime = agent.maddpgv2_lstm_target_actor.forward(actor_state_prime)
            
            # multiply by action_range
            curr_target_actor_actions_prime = T.mul(curr_target_actor_actions_prime, self.action_range)

            # # clamp by action_range
            # curr_target_actor_actions_prime = T.clamp(curr_target_actor_actions_prime, - self.action_range, self.action_range)

            # append actions to curr_target_actor_actions_prime_list
            curr_target_actor_actions_prime_list.append(curr_target_actor_actions_prime)
            
            # convert action_state to tensor 
            actor_state = T.tensor(np.concatenate((actor_state_arr[agent_index], actor_goals_arr[agent_index]), axis = -1), dtype = T.float).to(device)
            
            # feed actor_state tensor to actor to obtain actions
            curr_actor_actions = agent.maddpgv2_lstm_actor.forward(actor_state)
            
            # multiply by action_range
            curr_actor_actions = T.mul(curr_actor_actions, self.action_range)

            # # clamp by action_range
            # curr_actor_actions = T.clamp(curr_actor_actions, - self.action_range, self.action_range)

            # append actions to curr_actor_actions_list
            curr_actor_actions_list.append(curr_actor_actions)
            
            # append actions from past actor parameters from replay_buffer to past_actor_actions_list
            past_actor_actions_list.append(actor_action[agent_index])

        # concat actions in list
        curr_target_actor_actions_prime_cat = T.cat([action for action in curr_target_actor_actions_prime_list], dim = 1)
        curr_actor_actions_cat = T.cat([action for action in curr_actor_actions_list], dim = 1)
        past_actor_actions_cat = T.cat([action for action in past_actor_actions_list], dim = 1)
        
        # list to store metrics for logging
        actor_loss_list = []
        critic_loss_list = []
        actor_grad_norm_list = []
        critic_grad_norm_list = []
        actor_learning_rate_list = []
        critic_learning_rate_list = []

        # enumerate over agents
        for agent_index, agent in enumerate(self.maddpgv2_lstm_agents_list):
          
            # obtain target q value prime
            target_critic_q_value_prime = agent.maddpgv2_lstm_target_critic.forward(critic_state_prime, curr_target_actor_actions_prime_cat, critic_goals).flatten()
            
            # mask terminal target q values with 0
            target_critic_q_value_prime[terminal[:, 0]] = 0.0
            
            # obtain critic q value
            critic_q_value = agent.maddpgv2_lstm_critic.forward(critic_state, past_actor_actions_cat, critic_goals).flatten()
            
            # obtain td_target
            td_target = rewards[:, agent_index] + agent.discount_rate * target_critic_q_value_prime
            
            # critic loss is mean squared error between td_target and critic value 
            critic_loss = F.mse_loss(td_target, critic_q_value)
            
            # set critic model to train mode 
            agent.maddpgv2_lstm_critic.train()
             
            # reset gradients for critic model to zero
            agent.maddpgv2_lstm_critic.optimizer.zero_grad()
            
            # critic model back propagation
            critic_loss.backward(retain_graph = True)
            
            # visualise gradient flow
            print("Critic Gradients: ")
            plot_grad_flow(agent.maddpgv2_lstm_critic.named_parameters())

            # check if gradient clipping is needed
            if self.grad_clipping == True:
            
                # gradient norm clipping for critic model
                critic_grad_norm = T.nn.utils.clip_grad_norm_(agent.maddpgv2_lstm_critic.parameters(), max_norm = self.grad_norm_clip, norm_type = 2, error_if_nonfinite = True)

            # apply gradients to critic model
            agent.maddpgv2_lstm_critic.optimizer.step()
            
            # update critic learning rate scheduler
            agent.maddpgv2_lstm_critic.scheduler.step()

            # set critic to eval mode to calculate actor loss
            agent.maddpgv2_lstm_critic.eval()
            
            # set actor model to train mode 
            agent.maddpgv2_lstm_actor.train()
            
            # gradient ascent using critic value ouput as actor loss
            # loss is coupled with actor model from actor actions based on current policy 
            actor_loss = agent.maddpgv2_lstm_critic.forward(critic_state, curr_actor_actions_cat, critic_goals).flatten()
           
            # reduce mean across batch_size
            actor_loss = -T.mean(actor_loss)
            
            # reset gradients for actor model to zero
            agent.maddpgv2_lstm_actor.optimizer.zero_grad()

            # actor model back propagation
            actor_loss.backward(retain_graph = True, inputs = list(agent.maddpgv2_lstm_actor.parameters()))
            
            # visualise gradient flow
            print("Actor Gradients: ")
            plot_grad_flow(agent.maddpgv2_lstm_actor.named_parameters())

            # check if gradient clipping is needed
            if self.grad_clipping == True:
            
                # gradient norm clipping for critic model
                actor_grad_norm = T.nn.utils.clip_grad_norm_(agent.maddpgv2_lstm_actor.parameters(), max_norm = self.grad_norm_clip, norm_type = 2, error_if_nonfinite = True)

            # apply gradients to actor model
            agent.maddpgv2_lstm_actor.optimizer.step()
            
            # update critic learning rate scheduler
            agent.maddpgv2_lstm_actor.scheduler.step()

            # increment of apply_grad_counter
            self.apply_grad_counter += 1 
            
            # soft copy option: update target models based on user specified tau
            if self.update_target == None:
                
                # iterate over agents
                for agent in self.maddpgv2_lstm_agents_list:

                    # update target models
                    agent.update_maddpgv2_lstm_target_models()    
    
            # hard copy option every update_target steps
            else:
                
                if self.apply_grad_counter % self.update_target == 0: 
                
                    # iterate over agents
                    for agent in self.maddpgv2_lstm_agents_list:

                        # update target models
                        agent.update_maddpgv2_lstm_target_models()     

            # store actor and critic losses in list
            actor_loss_list.append(actor_loss.item())
            critic_loss_list.append(critic_loss.item())

            # check if there is grad clipping
            if self.grad_clipping == True:

                # append clipped gradients
                actor_grad_norm_list.append(actor_grad_norm.item())
                critic_grad_norm_list.append(critic_grad_norm.item())
               
            else:

                # append 0.0 if there is no gradient clipping
                actor_grad_norm_list.append(0.0)
                critic_grad_norm_list.append(0.0)

            # obtain learning rate
            actor_learning_rate_list.append(agent.maddpgv2_lstm_critic.scheduler.get_last_lr()[0])
            critic_learning_rate_list.append(agent.maddpgv2_lstm_actor.scheduler.get_last_lr()[0])

        return actor_loss_list, critic_loss_list, actor_grad_norm_list, critic_grad_norm_list, actor_learning_rate_list, critic_learning_rate_list
           
    def save_all_models(self):
        
        """ save weights for all models """
        
        print("saving models!")
        
        # iterate over num_agents
        for agent in self.maddpgv2_lstm_agents_list:
            
            # save each model
            agent.save_models()
    
    def load_all_models(self):
      
        """ load weights for all models """

        print("loading model!")

        # iterate over num_agents
        for agent in self.maddpgv2_lstm_agents_list:

            # load each model
            agent.load_models()