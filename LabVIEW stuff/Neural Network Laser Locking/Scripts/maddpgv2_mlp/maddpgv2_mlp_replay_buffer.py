# ==========================================================================================================================================================
# maddpgv2 mlp replay buffer class
# purpose: store memory of state, action, state_prime, reward, terminal flag and function to sample them similar to hindsight experience replay (her)
# ==========================================================================================================================================================

import math
import wave
import numpy as np
from scipy.fft import fft
from scipy.special import softmax
from utils.mlp_goal_replay_buffer import mlp_goal_replay_buffer
from utils.utils import dynamic_peak_finder

class maddpgv2_mlp_replay_buffer:
    
    def __init__(self, mem_size, num_agents, actions_dims, actor_input_dims, critic_input_dims, goal_dims, num_of_add_goals, goal_strategy, waveform_threshold):
        
        """ class constructor that initialises memory states attributes """

        # number of agents 
        self.num_agents = num_agents

        # dimension of goal of an agent
        self.goal_dims = goal_dims

        # number of additional goals
        self.num_of_add_goals = num_of_add_goals

        # strategy to sample additional goals
        # goal_strategy == "episode" : replay with num_of_add_goals random states coming from the same episode as the transition being replayed
        # goal_strategy == "future" : replay with num_of_add_goals random states which come from the same episode as the transition being replayed and were observed after it
        self.goal_strategy = goal_strategy

        # threshold for the waveform for the reward function
        self.waveform_threshold = waveform_threshold

        # list to store additional replay buffers
        self.add_goals_replay_buffer_list = []

        # replay buffer for original goal
        self.org_replay_buffer = mlp_goal_replay_buffer(mem_size = mem_size, num_agents = num_agents, actions_dims = actions_dims, actor_input_dims = actor_input_dims, 
                                                        critic_input_dims = critic_input_dims, goal_dims = goal_dims)

        # iterate over additional goals
        for i in range(self.num_of_add_goals):

            self.add_goals_replay_buffer_list.append(mlp_goal_replay_buffer(mem_size = mem_size, num_agents = num_agents, actions_dims = actions_dims, actor_input_dims = actor_input_dims, 
                                                                            critic_input_dims = critic_input_dims, goal_dims = goal_dims))

    def log(self, actor_state, actor_state_prime, org_actor_goals, critic_state, critic_state_prime, org_critic_goals, action, org_rewards, is_done):

        """ function to log memory and generate the log """

        # log the original experience in the replay buffer
        self.org_replay_buffer.log(actor_state = actor_state, actor_state_prime = actor_state_prime, actor_goals = org_actor_goals, critic_state = critic_state, 
                                   critic_state_prime = critic_state_prime, critic_goals = org_critic_goals, action = action, rewards = org_rewards, is_done = is_done)

        # iterate over num_of_add_goals
        for goal_index in range(self.num_of_add_goals):

            # fill the replay buffer with experiences from original replay buffer first
            # the goals, rewards will be updated accordingly with generate_her_replay_buffer once the episode has terminated
            self.add_goals_replay_buffer_list[goal_index].log(actor_state = actor_state, actor_state_prime = actor_state_prime, actor_goals = org_actor_goals, critic_state = critic_state, 
                                                              critic_state_prime = critic_state_prime, critic_goals = org_critic_goals, action = action, rewards = org_rewards, is_done = is_done)

    def generate_her_replay_buffer(self):

        """ function to generate the additonal goals replay buffer based on her """

        # check goal strategy
        if self.goal_strategy == "episode":

            # iterate over num_of_add_goals
            for add_goals_index in range(self.num_of_add_goals):

                # iterate over number of agents
                for agent_index in range(self.num_agents):

                    # for the case where the whole episode is continous in the replay buffer
                    if self.org_replay_buffer.ep_end_index > self.org_replay_buffer.ep_start_index:

                        # iterate over each time step in the episode
                        for time_step in range(self.org_replay_buffer.ep_start_index, self.org_replay_buffer.ep_end_index):

                            # select random index between start and end of episode
                            goal_index = np.random.randint(low = self.org_replay_buffer.ep_start_index, high = self.org_replay_buffer.ep_end_index)

                            # obtain reward for given goal
                            reward = self.goal_reward(add_goals_index = add_goals_index, agent_index = agent_index, time_step = time_step, goal_index = goal_index, goal = None)

                            # update reward to addtional replay buffer accordingly
                            self.add_goals_replay_buffer_list[add_goals_index].rewards_log[time_step][agent_index] = reward

                    # if episode is disjoint
                    elif self.org_replay_buffer.ep_end_index < self.org_replay_buffer.ep_start_index:

                        # iterate over each time step in the episode
                        for time_step in range(self.org_replay_buffer.ep_start_index, self.org_replay_buffer.mem_size):

                            # select random index between start and end of episode
                            goal_index = np.random.choice(np.array([np.random.randint(low = self.org_replay_buffer.ep_start_index, high = self.org_replay_buffer.mem_size), 
                                                                    np.random.randint(low = 0, high = self.org_replay_buffer.ep_end_index)]))

                            # obtain reward for given goal
                            reward = self.goal_reward(add_goals_index = add_goals_index, agent_index = agent_index, time_step = time_step, goal_index = goal_index, goal = None)

                            # update reward to addtional replay buffer accordingly
                            self.add_goals_replay_buffer_list[add_goals_index].rewards_log[time_step][agent_index] = reward

                        # iterate over each time step in the episode
                        for time_step in range(0, self.org_replay_buffer.ep_end_index):

                            # select random index between start and end of episode
                            goal_index = np.random.choice(np.array([np.random.randint(low = self.org_replay_buffer.ep_start_index, high = self.org_replay_buffer.mem_size), 
                                                                    np.random.randint(low = 0, high = self.org_replay_buffer.ep_end_index)]))

                            # obtain reward for given goal
                            reward = self.goal_reward(add_goals_index = add_goals_index, agent_index = agent_index, time_step = time_step, goal_index = goal_index, goal = None)

                            # update reward to addtional replay buffer accordingly
                            self.add_goals_replay_buffer_list[add_goals_index].rewards_log[time_step][agent_index] = reward

        # check goal strategy
        elif self.goal_strategy == "future":

            # iterate over num_of_add_goals
            for add_goals_index in range(self.num_of_add_goals):

                # iterate over number of agents
                for agent_index in range(self.num_agents):

                    # for the case where the whole episode is continous in the replay buffer
                    if self.org_replay_buffer.ep_end_index > self.org_replay_buffer.ep_start_index:

                        # iterate over each time step in the episode
                        for time_step in range(self.org_replay_buffer.ep_start_index, self.org_replay_buffer.ep_end_index):

                            # select random index between start and end of episode
                            goal_index = np.random.randint(low = time_step, high = self.org_replay_buffer.ep_end_index)

                            # obtain reward for given goal
                            reward = self.goal_reward(add_goals_index = add_goals_index, agent_index = agent_index, time_step = time_step, goal_index = goal_index, goal = None)

                            # update reward to addtional replay buffer accordingly
                            self.add_goals_replay_buffer_list[add_goals_index].rewards_log[time_step][agent_index] = reward

                    # if episode is disjoint
                    elif self.org_replay_buffer.ep_end_index < self.org_replay_buffer.ep_start_index:

                        # iterate over each time step in the episode
                        for time_step in range(self.org_replay_buffer.ep_start_index, self.org_replay_buffer.mem_size):

                            # select random index between start and end of episode
                            goal_index = np.random.choice(np.array([np.random.randint(low = time_step, high = self.org_replay_buffer.mem_size), 
                                                                    np.random.randint(low = 0, high = self.org_replay_buffer.ep_end_index)]))

                            # obtain reward for given goal
                            reward = self.goal_reward(add_goals_index = add_goals_index, agent_index = agent_index, time_step = time_step, goal_index = goal_index, goal = None)

                            # update reward to addtional replay buffer accordingly
                            self.add_goals_replay_buffer_list[add_goals_index].rewards_log[time_step][agent_index] = reward

                        # iterate over each time step in the episode
                        for time_step in range(0, self.org_replay_buffer.ep_end_index):

                            # select random index between start and end of episode
                            goal_index = np.random.randint(low = time_step, high = self.org_replay_buffer.ep_end_index)

                            # obtain reward for given goal
                            reward = self.goal_reward(add_goals_index = add_goals_index, agent_index = agent_index, time_step = time_step, goal_index = goal_index, goal = None)

                            # update reward to addtional replay buffer accordingly
                            self.add_goals_replay_buffer_list[add_goals_index].rewards_log[time_step][agent_index] = reward

    def goal_reward(self, add_goals_index, agent_index, time_step, goal_index, goal):

        """ function that generates reward for a given goal based on episode from original replay buffer """

        # initialise reward to zero
        rew = 0

        # check to use goal_index
        if goal_index is not None and goal is None:

            # obtain goal for agent drones from given goal_index from org_replay_buffer
            # goal here for good agent is the time_elapsed in episode
            goal = self.org_replay_buffer.actor_state_log_list[agent_index][goal_index][1:self.goal_dims + 1]
            goal = goal - goal.mean()
            goal[abs(goal) <= self.waveform_threshold] = 0
            goal = dynamic_peak_finder(goal)
            
        # store goal in additional replay buffer
        self.add_goals_replay_buffer_list[add_goals_index].actor_goals_log_list[agent_index][time_step] = goal
        self.add_goals_replay_buffer_list[add_goals_index].critic_goals_log[time_step][agent_index * self.goal_dims: agent_index * self.goal_dims + self.goal_dims] = goal

        # obtain wf_state
        wf_state = self.add_goals_replay_buffer_list[add_goals_index].actor_state_log_list[agent_index][time_step][1:self.goal_dims + 1]
        wf_state = wf_state - wf_state.mean()
        wf_state[abs(wf_state) <= self.waveform_threshold] = 0
        wf_state = dynamic_peak_finder(wf_state)

        # correlate
        corr_auto = np.correlate(wf_state, goal, "full")

        # cal reward
        rew += np.sum(np.absolute(corr_auto))

        return rew

    def sample_log(self, batch_size):

        """ function to sample experience from original and additional replay buffers """

        # obtain truncated batch size for each replay buffer
        batch_size_per_buffer = int(batch_size / (1 + self.num_of_add_goals))

        # list for batch_size_per_buffer
        batch_size_per_buffer_list = [batch_size_per_buffer for i in range(1 + self.num_of_add_goals)]

        # check if batch_size_per_buffer sums up to batch_size
        if sum(batch_size_per_buffer_list) < batch_size:

            # obtain difference
            diff = batch_size - sum(batch_size_per_buffer_list)

            # obtain array of indexes to add 1 to batch_size_per_buffer
            extra_index_arr = np.random.choice(a = 1 + self.num_of_add_goals, size = diff, replace = False)

            # iterate over extra_index_arr
            for index in extra_index_arr:

                # add 1 to index in batch_size_per_buffer_list
                batch_size_per_buffer_list[index] += 1

        # obtain batch of experience from original replay buffer
        org_actor_state_log_list, org_actor_state_prime_log_list, org_actor_action_log_list, org_actor_goals_log_list, org_critic_state_log, \
        org_critic_state_prime_log, org_critic_goals_log, org_rewards_log, org_terminal_log = self.org_replay_buffer.sample_log(batch_size = batch_size_per_buffer_list[0])
        
        # generate all list as numpy arrays except critic_state and critic_state_prime
        org_actor_state_log = np.array(org_actor_state_log_list, dtype = np.float32)
        org_actor_state_prime_log = np.array(org_actor_state_prime_log_list, dtype = np.float32)
        org_actor_action_log = np.array(org_actor_action_log_list, dtype = np.float32)
        org_actor_goals_log = np.array(org_actor_goals_log_list, dtype = np.float32)
        
        # iterate over num_of_add_goals
        for goal_index in range(self.num_of_add_goals):

            # obtain batch of experience from additional replay buffer
            actor_state_log_list, actor_state_prime_log_list, actor_action_log_list, actor_goals_log_list, critic_state_log, critic_state_prime_log, critic_goals_log, rewards_log, terminal_log = \
            self.add_goals_replay_buffer_list[goal_index].sample_log(batch_size = batch_size_per_buffer_list[goal_index + 1])

            # generate all list as numpy arrays except critic_state and critic_state_prime
            actor_state_log = np.array(actor_state_log_list, dtype = np.float32)
            actor_state_prime_log = np.array(actor_state_prime_log_list, dtype = np.float32)
            actor_action_log = np.array(actor_action_log_list, dtype = np.float32)
            actor_goals_log = np.array(actor_goals_log_list, dtype = np.float32)

            # concatenate the arrays
            org_actor_state_log = np.concatenate((org_actor_state_log, actor_state_log), axis = 1)
            org_actor_state_prime_log = np.concatenate((org_actor_state_prime_log, actor_state_prime_log), axis = 1)
            org_actor_action_log = np.concatenate((org_actor_action_log, actor_action_log), axis = 1)
            org_actor_goals_log = np.concatenate((org_actor_goals_log, actor_goals_log), axis = 1)
            org_critic_goals_log = np.concatenate((org_critic_goals_log, critic_goals_log), axis = 0)
            org_critic_state_log = np.concatenate((org_critic_state_log, critic_state_log), axis = 0)
            org_critic_state_prime_log = np.concatenate((org_critic_state_prime_log, critic_state_prime_log), axis = 0)
            org_rewards_log = np.concatenate((org_rewards_log, rewards_log), axis = 0)
            org_terminal_log = np.concatenate((org_terminal_log, terminal_log), axis = 0)

        return org_actor_state_log, org_actor_state_prime_log, org_actor_action_log, org_actor_goals_log, org_critic_state_log, org_critic_state_prime_log, org_critic_goals_log, org_rewards_log, \
               org_terminal_log

    def load_replay_buffer(self, replay_buffer_list):

        """ function to load replay buffer from a list of dicts """

        # load original replay buffer
        self.org_replay_buffer.load_replay_buffer(replay_buffer_list[0]) 

        # iterate over number of additional goals
        for i in range(self.num_of_add_goals):

            # load addtional replay buffer
            self.add_goals_replay_buffer_list[i].load_replay_buffer(replay_buffer_list[i + 1])