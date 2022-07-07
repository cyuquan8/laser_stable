# ==========================================================================================================================================================
# mlp replay buffer class
# purpose: generic replay buffer to store memory of state, action, state_prime, reward, terminal flag
# ==========================================================================================================================================================

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

    def load_replay_buffer(self, replay_buffer_dict):

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