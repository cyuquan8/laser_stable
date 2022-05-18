# ==========================================================================================================================================================
# maddpgv2 mlp replay buffer class
# purpose: store memory of state, action, state_prime, reward, terminal flag and function to sample them similar to hindsight experience replay (her)
# ==========================================================================================================================================================

import math
import numpy as np

class mlp_goal_replay_buffer:
    
    def __init__(self, mem_size, num_agents, actions_dims, actor_input_dims, critic_input_dims, goal_dims):
        
        """ class constructor that initialises memory states attributes """
        
        # bound for memory log
        self.mem_size = mem_size
        
        # counter for memory logged
        self.mem_counter = 0 
        
        # track start and end index (non-inclusive) of latest episode
        self.ep_start_index = 0
        self.ep_end_index = 0

        # boolean to track if latest logged experience tuple is terminal
        self.is_ep_terminal = False

        # number of agents
        self.num_agents = num_agents
        
        # dimension of goal of an agent
        self.goal_dims = goal_dims

        # dimensions of action space
        self.actions_dims = actions_dims
        
        # reward_log is list of reward from num_agents of actors
        # terminal_log indicates if episode is terminated
        self.rewards_log = np.zeros((self.mem_size, self.num_agents)) 
        self.terminal_log = np.zeros((self.mem_size, self.num_agents), dtype = bool)
        
        # list to store num_agents of each actor log of state, state_prime, actions and goals 
        self.actor_state_log_list = []
        self.actor_state_prime_log_list = []
        self.actor_action_log_list = []
        self.actor_goals_log_list = []

        # list to store graph data representation of critic state, state prime 
        self.critic_state_log = np.zeros((self.mem_size, critic_input_dims)) 
        self.critic_state_prime_log = np.zeros((self.mem_size, critic_input_dims)) 

        # list to store goals of agents
        self.critic_goals_log = np.zeros((self.mem_size, self.goal_dims * self.num_agents)) 
        
        # iterate over num_agents
        for actor_index in range(self.num_agents):
            
            # append each actor log to list
            # actor_state and actor_state_prime are local observations of environment by each actor
            self.actor_state_log_list.append(np.zeros((self.mem_size, actor_input_dims[actor_index])))
            self.actor_state_prime_log_list.append(np.zeros((self.mem_size, actor_input_dims[actor_index])))
            self.actor_action_log_list.append(np.zeros((self.mem_size, self.actions_dims)))
            self.actor_goals_log_list.append(np.zeros((self.mem_size, self.goal_dims)))            
    
    def log(self, actor_state, actor_state_prime, actor_goals, critic_state, critic_state_prime, critic_goals, action, rewards, is_done):
        
        """ function to log memory """
        
        # index for logging. based on first in first out
        index = self.mem_counter % self.mem_size
        
        # iterate over num_agents
        for actor_index in range(self.num_agents):
            
            # log actor_state, actor_state_prime, motor and communication action and goal for each actor
            self.actor_state_log_list[actor_index][index] = actor_state[actor_index]
            self.actor_state_prime_log_list[actor_index][index] = actor_state_prime[actor_index]
            self.actor_action_log_list[actor_index][index] = action[actor_index]
            self.actor_goals_log_list[actor_index][index] = actor_goals[actor_index]

        # log critic_fc_state, critic_fc_state_prime, rewards and terminal flag
        self.critic_state_log[index] = critic_state
        self.critic_state_prime_log[index] = critic_state_prime
        self.critic_goals_log[index] = critic_goals
        self.rewards_log[index] = rewards
        self.terminal_log[index] = is_done
        
        # increment counter
        self.mem_counter += 1
        self.ep_end_index = (self.ep_end_index + 1) % self.mem_size

        # check if logged episode is terminal
        if np.any(is_done) == True:

            # update is_ep_terminal
            self.is_ep_terminal = True

        else: 

            # update is_ep_terminal
            self.is_ep_terminal = False

        # calculate ep_start_index
        if np.any(self.terminal_log[index - 1]) == True:

            self.ep_start_index = index
    
    def sample_log(self, batch_size, rng = np.random.default_rng(69)):
        
        """ function to randomly sample a batch of memory """
        
        # select amongst memory logs that is filled
        max_mem = min(self.mem_counter, self.mem_size)
        
        # randomly select memory from logs
        batch = rng.choice(max_mem, batch_size, replace = False)
        
        # initialise list for actor_state, actor_state_prime, actions, critic_state, critic_state_prime, critic_goals
        actor_state_log_list = []
        actor_state_prime_log_list = []
        actor_action_log_list = []
        actor_goals_log_list = []

        # iterate over num_agents
        for actor_index in range(self.num_agents):
            
            # obtain corresponding actor_state, actor_state_prime and actions
            actor_state_log_list.append(self.actor_state_log_list[actor_index][batch])
            actor_state_prime_log_list.append(self.actor_state_prime_log_list[actor_index][batch])
            actor_action_log_list.append(self.actor_action_log_list[actor_index][batch])
            actor_goals_log_list.append(self.actor_goals_log_list[actor_index][batch])
        
        # obtain corresponding rewards, terminal flag
        rewards_log = self.rewards_log[batch]
        terminal_log = self.terminal_log[batch]
        critic_state_log = self.critic_state_log[batch] 
        critic_state_prime_log = self.critic_state_prime_log[batch]
        critic_goals_log = self.critic_goals_log[batch]
        
        return actor_state_log_list, actor_state_prime_log_list, actor_action_log_list, actor_goals_log_list, critic_state_log, critic_state_prime_log, critic_goals_log, rewards_log, terminal_log

    def load_replay_buffer(replay_buffer_dict):

        """ function to load replay buffer data from dictionary """ 

        # reload logs
        self.rewards_log = replay_buffer_dict["rewards_log"]
        self.terminal_log = replay_buffer_dict["terminal_log"]
        self.actor_state_log_list = replay_buffer_dict["actor_state_log_list"]
        self.actor_state_prime_log_list = replay_buffer_dict["actor_state_prime_log_list"]
        self.actor_action_log_list = replay_buffer_dict["actor_action_log_list"]
        self.actor_goals_log_list = replay_buffer_dict["actor_goals_log_list"]
        self.critic_state_log = replay_buffer_dict["critic_state_log"] 
        self.critic_state_prime_log = replay_buffer_dict["critic_state_prime_log"] 
        self.critic_goals_log = replay_buffer_dict["critic_goals_log"]         

class maddpgv2_mlp_replay_buffer:
    
    def __init__(self, mem_size, num_agents, actions_dims, actor_input_dims, critic_input_dims, goal_dims, num_of_add_goals, goal_strategy):
        
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
            goal = self.org_replay_buffer.actor_state_log_list[agent_index][goal_index][0]

        # store goal in additional replay buffer
        self.add_goals_replay_buffer_list[add_goals_index].actor_goals_log_list[agent_index][time_step] = goal
        self.add_goals_replay_buffer_list[add_goals_index].critic_goals_log[time_step][agent_index * self.goal_dims: agent_index * self.goal_dims + self.goal_dims] = goal



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
        org_actor_state_log_list, org_actor_state_prime_log_list, org_actor_c_action_log_list, org_actor_goals_log_list, org_critic_state_log, \
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

    def load_replay_buffer(replay_buffer_list):

        """ function to load replay buffer from a list of dicts """

        # load original replay buffer
        self.org_replay_buffer.load_replay_buffer(replay_buffer_list[0]) 

        # iterate over number of additional goals
        for i in range(self.num_of_add_goals):

            # load addtional replay buffer
            self.add_goals_replay_buffer_list[i].load_replay_buffer(replay_buffer_list[i + 1])