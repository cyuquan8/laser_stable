# ==========================================================================================================================================================
# main train test
# purpose: main training function that interacts with environment through saved files
# ==========================================================================================================================================================

import os
import shutil
import sys
import copy
import math
import numpy as np
import torch as T
import pandas as pd
import matplotlib.pyplot as plt
from torch.utils.tensorboard import SummaryWriter 
from maddpgv2_mlp.maddpgv2_mlp import maddpgv2_mlp
from maddpgv2_lstm.maddpgv2_lstm import maddpgv2_lstm
from utils.utils import reward, is_terminating, terminating_condition, dynamic_peak_finder
from scipy.fft import fft
from scipy.special import softmax
from time import time


# general options
SCENARIO_NAME												= "rb_laser_locking" 
MODEL 														= "maddpgv2_lstm" 
#MODE 														= "train"
TRAINING_NAME 												= SCENARIO_NAME + "_" + MODEL + "_50_time_steps"

CSV_LOG_DIRECTORY											= "csv_log" + '/' + SCENARIO_NAME
LABVIEW_LOG_DIRECTORY										= "labview_log" +  '/' + SCENARIO_NAME
TENSORBOARD_LOG_DIRECTORY									= "tensorboard_log" +  '/' + SCENARIO_NAME + '/' + TRAINING_NAME
GOAL_PATH													= "goal.csv"
CONFIG 														= np.loadtxt(fname = ".\\config.txt", delimiter = ',')
NUMBER_OF_EPISODES 											= int(CONFIG[0])
EPISODE_TIME_STEP_LIMIT										= int(CONFIG[1])
SAVE_MODEL_RATE 											= 50
SAVE_CSV_LOG												= True

# env options
NUMBER_OF_AGENTS 											= 1
GOAL_DIMENSIONS												= 2000
STATE_DIMENSIONS 											= GOAL_DIMENSIONS + 3
CURRENT_ACTION_DIMENSIONS									= 1
RAMP_ACTION_DIMENSIONS										= 2
ACTION_DIMENSIONS  											= CURRENT_ACTION_DIMENSIONS + RAMP_ACTION_DIMENSIONS
ACTION_NOISE												= 1.0
EXPONENTIAL_NOISE_DECAY										= True
DECAY_CONSTANT												= 0.001
ACTION_RANGE												= 1.0
REWARD_THRESHOLD											= 10
WAVEFORM_PROMINENCE 	 									= (0.2, None)
REWARD_MULTIPLIER_CONSTANT									= 1000
NUMBER_OF_PEAKS_THRESHOLD									= 3
CURRENT_LOWER_BOUND											= 0.121
CURRENT_UPPER_BOUND											= 0.125
RAMP_LOWER_BOUND											= 1
RAMP_UPPER_BOUND 											= 5

# define hyperparameters for maddpgv2_mlp
MADDPGV2_MLP_DISCOUNT_RATE 									= 0.99
MADDPGV2_MLP_LEARNING_RATE_ACTOR 							= 0.0005
MADDPGV2_MLP_LEARNING_RATE_CRITIC 							= 0.0005
MADDPGV2_MLP_OPTIMIZER 										= "adam"
MADDPGV2_MLP_ACTOR_LEARNING_RATE_SCHEDULER 					= "cosine_annealing_with_warm_restarts"
MADDPGV2_MLP_ACTOR_COSINE_ANNEALING_RESTART_ITERATION		= 10000
MADDPGV2_MLP_ACTOR_COSINE_ANNEALING_MINIMUM_LEARNING_RATE	= 0.00005
MADDPGV2_MLP_CRITIC_LEARNING_RATE_SCHEDULER 				= "cosine_annealing_with_warm_restarts"
MADDPGV2_MLP_CRITIC_COSINE_ANNEALING_RESTART_ITERATION		= 10000
MADDPGV2_MLP_CRITIC_COSINE_ANNEALING_MINIMUM_LEARNING_RATE	= 0.00005
MADDPGV2_MLP_ACTOR_DROPOUT									= 0
MADDPGV2_MLP_CRITIC_DROPOUT									= 0
MADDPGV2_MLP_TAU 											= 0.01	  
MADDPGV2_MLP_MEMORY_SIZE 									= 10000
MADDPGV2_MLP_BATCH_SIZE 									= 64
MADDPGV2_MLP_UPDATE_TARGET 									= None
MADDPGV2_MLP_GRADIENT_CLIPPING								= True
MADDPGV2_MLP_GRADIENT_NORM_CLIP								= 10
MADDPGV2_MLP_GOAL 											= np.loadtxt(fname = GOAL_PATH, delimiter = ",")
MADDPGV2_MLP_ADDITIONAL_GOALS								= 0
MADDPGV2_MLP_GOAL_STRATEGY									= "future"

MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS						= [512, 128]
MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS							= [512, 128]

# define hyperparameters for maddpv2_lstm
MADDPGV2_LSTM_DISCOUNT_RATE 								= 0.99
MADDPGV2_LSTM_LEARNING_RATE_ACTOR 							= 0.0001
MADDPGV2_LSTM_LEARNING_RATE_CRITIC 							= 0.00025
MADDPGV2_LSTM_OPTIMIZER 									= "adam"
MADDPGV2_LSTM_ACTOR_LEARNING_RATE_SCHEDULER 				= "cosine_annealing_with_warm_restarts"
MADDPGV2_LSTM_ACTOR_COSINE_ANNEALING_RESTART_ITERATION		= 10000
MADDPGV2_LSTM_ACTOR_COSINE_ANNEALING_MINIMUM_LEARNING_RATE	= 0.00005
MADDPGV2_LSTM_CRITIC_LEARNING_RATE_SCHEDULER 				= "cosine_annealing_with_warm_restarts"
MADDPGV2_LSTM_CRITIC_COSINE_ANNEALING_RESTART_ITERATION		= 10000
MADDPGV2_LSTM_CRITIC_COSINE_ANNEALING_MINIMUM_LEARNING_RATE	= 0.000125
MADDPGV2_LSTM_ACTOR_DROPOUT									= 0
MADDPGV2_LSTM_CRITIC_DROPOUT								= 0
MADDPGV2_LSTM_TAU 											= 0.01	  
MADDPGV2_LSTM_MEMORY_SIZE 									= 10000
MADDPGV2_LSTM_BATCH_SIZE 									= 256
MADDPGV2_LSTM_UPDATE_TARGET 								= None
MADDPGV2_LSTM_GRADIENT_CLIPPING								= True
MADDPGV2_LSTM_GRADIENT_NORM_CLIP							= 10
MADDPGV2_LSTM_GOAL 											= np.loadtxt(fname = GOAL_PATH, delimiter = ",")
MADDPGV2_LSTM_ADDITIONAL_GOALS								= 0
MADDPGV2_LSTM_GOAL_STRATEGY									= "future"

MADDPGV2_LSTM_ACTOR_INPUT_SIZE								= 500
MADDPGV2_LSTM_ACTOR_SEQUENCE_LENGTH							= math.ceil((STATE_DIMENSIONS + GOAL_DIMENSIONS) / MADDPGV2_LSTM_ACTOR_INPUT_SIZE)
MADDPGV2_LSTM_ACTOR_HIDDEN_SIZE								= MADDPGV2_LSTM_ACTOR_INPUT_SIZE
MADDPGV2_LSTM_ACTOR_NUMBER_OF_LAYERS						= 1
MADDPGV2_LSTM_CRITIC_INPUT_SIZE								= 500
MADDPGV2_LSTM_CRITIC_SEQUENCE_LENGTH						= math.ceil((STATE_DIMENSIONS + GOAL_DIMENSIONS + ACTION_DIMENSIONS) * NUMBER_OF_AGENTS / MADDPGV2_LSTM_CRITIC_INPUT_SIZE)
MADDPGV2_LSTM_CRITIC_HIDDEN_SIZE							= MADDPGV2_LSTM_CRITIC_INPUT_SIZE
MADDPGV2_LSTM_CRITIC_NUMBER_OF_LAYERS						= 1

def train_test(MODE):
	
	""" function to execute experiments to train or test models based on different algorithms """

	# check model
	if MODEL == "maddpgv2_mlp":

		maddpgv2_mlp_agents = maddpgv2_mlp(mode = MODE, scenario_name = SCENARIO_NAME, training_name = TRAINING_NAME, discount_rate = MADDPGV2_MLP_DISCOUNT_RATE, 
										   lr_actor = MADDPGV2_MLP_LEARNING_RATE_ACTOR, lr_critic = MADDPGV2_MLP_LEARNING_RATE_CRITIC, optimizer = MADDPGV2_MLP_OPTIMIZER, 
										   actor_lr_scheduler = MADDPGV2_MLP_ACTOR_LEARNING_RATE_SCHEDULER, critic_lr_scheduler = MADDPGV2_MLP_CRITIC_LEARNING_RATE_SCHEDULER, num_agents = NUMBER_OF_AGENTS, 
										   actor_dropout_p = MADDPGV2_MLP_ACTOR_DROPOUT, critic_dropout_p = MADDPGV2_MLP_CRITIC_DROPOUT, 
										   state_fc_input_dims = [STATE_DIMENSIONS for i in range(NUMBER_OF_AGENTS)], actor_state_fc_output_dims = MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS, 
										   critic_state_fc_output_dims = MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS, action_dims = ACTION_DIMENSIONS, 
										   goal_fc_input_dims = [GOAL_DIMENSIONS for i in range(NUMBER_OF_AGENTS)], tau = MADDPGV2_MLP_TAU, actor_action_noise = ACTION_NOISE, 
										   actor_action_range = ACTION_RANGE, mem_size = MADDPGV2_MLP_MEMORY_SIZE, batch_size = MADDPGV2_MLP_BATCH_SIZE, update_target = MADDPGV2_MLP_UPDATE_TARGET, 
										   grad_clipping = MADDPGV2_MLP_GRADIENT_CLIPPING, grad_norm_clip = MADDPGV2_MLP_GRADIENT_NORM_CLIP, num_of_add_goals = MADDPGV2_MLP_ADDITIONAL_GOALS, 
										   goal_strategy = MADDPGV2_MLP_GOAL_STRATEGY, waveform_prominence = WAVEFORM_PROMINENCE, number_of_peaks_threshold = NUMBER_OF_PEAKS_THRESHOLD, 
										   reward_multiplier_constant = REWARD_MULTIPLIER_CONSTANT, actor_lr_scheduler_T_0 = MADDPGV2_MLP_ACTOR_COSINE_ANNEALING_RESTART_ITERATION, 
										   actor_lr_scheduler_eta_min = MADDPGV2_MLP_ACTOR_COSINE_ANNEALING_MINIMUM_LEARNING_RATE, critic_lr_scheduler_T_0 = MADDPGV2_MLP_CRITIC_COSINE_ANNEALING_RESTART_ITERATION, 
										   critic_lr_scheduler_eta_min = MADDPGV2_MLP_CRITIC_COSINE_ANNEALING_MINIMUM_LEARNING_RATE)

	elif MODEL == "maddpgv2_lstm":

		maddpgv2_lstm_agents = maddpgv2_lstm(mode = MODE, scenario_name = SCENARIO_NAME, training_name = TRAINING_NAME, discount_rate = MADDPGV2_LSTM_DISCOUNT_RATE, lr_actor = MADDPGV2_LSTM_LEARNING_RATE_ACTOR, 
											 lr_critic = MADDPGV2_LSTM_LEARNING_RATE_CRITIC, optimizer = MADDPGV2_LSTM_OPTIMIZER, actor_lr_scheduler = MADDPGV2_LSTM_ACTOR_LEARNING_RATE_SCHEDULER, 
											 critic_lr_scheduler = MADDPGV2_LSTM_CRITIC_LEARNING_RATE_SCHEDULER, num_agents = NUMBER_OF_AGENTS, actor_dropout_p = MADDPGV2_LSTM_ACTOR_DROPOUT, 
											 critic_dropout_p = MADDPGV2_LSTM_CRITIC_DROPOUT, state_dims = [STATE_DIMENSIONS for i in range(NUMBER_OF_AGENTS)], 
											 goal_dims = [GOAL_DIMENSIONS for i in range(NUMBER_OF_AGENTS)], actor_lstm_sequence_length = [MADDPGV2_LSTM_ACTOR_SEQUENCE_LENGTH for i in range(NUMBER_OF_AGENTS)], 
											 actor_lstm_input_size = [MADDPGV2_LSTM_ACTOR_INPUT_SIZE for i in range(NUMBER_OF_AGENTS)], 
											 actor_lstm_hidden_size = [MADDPGV2_LSTM_ACTOR_HIDDEN_SIZE for i in range(NUMBER_OF_AGENTS)], 
											 actor_lstm_num_layers = [MADDPGV2_LSTM_ACTOR_NUMBER_OF_LAYERS for i in range(NUMBER_OF_AGENTS)], action_dims = ACTION_DIMENSIONS, 
											 critic_lstm_sequence_length = [MADDPGV2_LSTM_CRITIC_SEQUENCE_LENGTH for i in range(NUMBER_OF_AGENTS)], 
											 critic_lstm_input_size = [MADDPGV2_LSTM_CRITIC_INPUT_SIZE for i in range(NUMBER_OF_AGENTS)], 
											 critic_lstm_hidden_size = [MADDPGV2_LSTM_CRITIC_HIDDEN_SIZE for i in range(NUMBER_OF_AGENTS)], 
											 critic_lstm_num_layers = [MADDPGV2_LSTM_CRITIC_NUMBER_OF_LAYERS for i in range(NUMBER_OF_AGENTS)], tau = MADDPGV2_LSTM_TAU, actor_action_noise = ACTION_NOISE, 
											 actor_action_range = ACTION_RANGE, mem_size = MADDPGV2_LSTM_MEMORY_SIZE, batch_size = MADDPGV2_LSTM_BATCH_SIZE, update_target = MADDPGV2_LSTM_UPDATE_TARGET, 
											 grad_clipping = MADDPGV2_LSTM_GRADIENT_CLIPPING, grad_norm_clip = MADDPGV2_LSTM_GRADIENT_NORM_CLIP, num_of_add_goals = MADDPGV2_LSTM_ADDITIONAL_GOALS, 
											 goal_strategy = MADDPGV2_LSTM_GOAL_STRATEGY, waveform_prominence = WAVEFORM_PROMINENCE, number_of_peaks_threshold = NUMBER_OF_PEAKS_THRESHOLD, 
											 reward_multiplier_constant = REWARD_MULTIPLIER_CONSTANT, actor_lr_scheduler_T_0 = MADDPGV2_LSTM_ACTOR_COSINE_ANNEALING_RESTART_ITERATION, 
											 actor_lr_scheduler_eta_min = MADDPGV2_LSTM_ACTOR_COSINE_ANNEALING_MINIMUM_LEARNING_RATE, 
											 critic_lr_scheduler_T_0 = MADDPGV2_LSTM_CRITIC_COSINE_ANNEALING_RESTART_ITERATION, 
											 critic_lr_scheduler_eta_min = MADDPGV2_LSTM_CRITIC_COSINE_ANNEALING_MINIMUM_LEARNING_RATE)

	# if log directory for tensorboard exist
	if os.path.exists(TENSORBOARD_LOG_DIRECTORY):
		
		# remove entire directory
		shutil.rmtree(TENSORBOARD_LOG_DIRECTORY)

	# generate writer for tensorboard logging
	writer = SummaryWriter(log_dir = TENSORBOARD_LOG_DIRECTORY)

	# variables to track the sum of wins
	sum_wins = 0

	# list to store metrics to be converted to csv for postprocessing
	sum_wins_list = []
	avg_actor_loss_list = []
	avg_critic_loss_list = []
	avg_actor_grad_norm_list = []
	avg_critic_grad_norm_list = []
	avg_actor_learning_rate_list = []
	avg_critic_learning_rate_list = []
	avg_reward_list = []

	# set up agent actor goals for maddpgv2_mlp
	if MODEL == "maddpgv2_mlp":

		goal = MADDPGV2_MLP_GOAL
		norm_goal = np.linalg.norm(goal)
		goal = goal - goal.mean()
		goal = goal / max(abs(goal))
		goal = dynamic_peak_finder(x = goal, prominence = WAVEFORM_PROMINENCE, number_of_peaks_threshold = NUMBER_OF_PEAKS_THRESHOLD)
	
	# set up agent actor goals for maddpgv2_lstm
	elif MODEL == "maddpgv2_lstm":

		goal = MADDPGV2_LSTM_GOAL
		norm_goal = np.linalg.norm(goal)
		goal = goal - goal.mean()
		goal = goal / max(abs(goal))
		goal = dynamic_peak_finder(x = goal, prominence = WAVEFORM_PROMINENCE, number_of_peaks_threshold = NUMBER_OF_PEAKS_THRESHOLD)

	# iterate over number of episodes
	for eps in range(1, NUMBER_OF_EPISODES + 1): 

		# boolean to check if episode is terminal
		is_terminal = 0

		# variable to track terminal condition
		terminal_condition = 0

		# track time step
		ep_time_step = 0

		# print episode number 
		print("episode " + str(eps) + ":") 

		# boolean to track if data is obtained from disk
		is_data_obtained = False

		# repeatedly attempt to obtain data
		while not is_data_obtained:

			# # debugging messages
			# print(f"Fetching state from episode {eps} time step {ep_time_step} ... ")

			try:

				# open file
				actor_states = np.loadtxt(fname = LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(ep_time_step) + "_state.csv", delimiter = ",")
		
				# make sure actor states is not empty
				if len(actor_states) != STATE_DIMENSIONS:
					
					# raise exception with debugging messages
					raise ValueError(f"State from episode {eps} time step {ep_time_step} is empty / wrong length ... ")  

				# obtain initial state from labview
				actor_states = np.expand_dims(actor_states, axis = 0)

			except KeyboardInterrupt:
				
				# abort
				sys.exit()

			except:

				pass

			else: 

				# debugging messages
				print(f"Fetched state from episode {eps} time step {ep_time_step} ... ")

				# update boolean variable to exit while loop
				is_data_obtained = True

		# set up agent actor goals for maddpgv2_lstm or maddpgv2_lstm
		if MODEL == "maddpgv2_mlp" or MODEL == "maddpgv2_lstm":
			
			actor_goals = np.array([goal for i in range(NUMBER_OF_AGENTS)])
			critic_goals = np.array([[goal] for i in range(NUMBER_OF_AGENTS)]).reshape(1, -1)
			
		# variables to track metrics
		sum_actor_loss = 0
		sum_critic_loss = 0
		sum_actor_grad_norm = 0
		sum_critic_grad_norm = 0
		sum_actor_learning_rate = 0
		sum_critic_learning_rate = 0
		sum_reward = 0

		# check if exponential noise decay is used
		if EXPONENTIAL_NOISE_DECAY == True:

			# check model
			if MODEL == "maddpgv2_mlp":

				# iterate over agents
				for i in range(NUMBER_OF_AGENTS):

					maddpgv2_mlp_agents.maddpgv2_mlp_agents_list[i].actor_action_noise = ACTION_NOISE * np.exp(- DECAY_CONSTANT * eps)
			
			elif MODEL == "maddpgv2_lstm":

				# iterate over agents
				for i in range(NUMBER_OF_AGENTS):

					maddpgv2_lstm_agents.maddpgv2_lstm_agents_list[i].actor_action_noise = ACTION_NOISE * np.exp(- DECAY_CONSTANT * eps)

		# iterate till episode terminates
		while is_terminal == 0:

			# obtain actions for maddpgv2_mlp_agents or maddpgv2_lstm_agents
			if MODEL == "maddpgv2_mlp":
		
				# actor_states concatenated with actor_goals
				actor_states_p_goal = np.concatenate((actor_states, actor_goals), axis = -1)

				# generate actions during testing
				actions = maddpgv2_mlp_agents.select_actions(mode = MODE, actor_state_list = actor_states_p_goal)

			elif MODEL == "maddpgv2_lstm":

				# actor_states concatenated with actor_goals
				actor_states_p_goal = np.concatenate((actor_states, actor_goals), axis = -1)

				# generate actions during testing
				actions = maddpgv2_lstm_agents.select_actions(mode = MODE, actor_state_list = actor_states_p_goal)
	
			# debugging messages
			print(f"Saving actions from episode {eps} time step {ep_time_step} ... ")

			# iterate over agents
			for i in range(NUMBER_OF_AGENTS):
				
				# initialise empty list for labview actions
				labview_actions_list = []

				# check if implementing action for current 
				if ((actor_states[i][0] + actions[i][0] * 0.001) <= CURRENT_LOWER_BOUND) or ((actor_states[i][0] + actions[i][0] * 0.001) >= CURRENT_UPPER_BOUND):
					
					# append action of 0 to csv for labview to open and implement if new current exceeds bound 
					labview_actions_list.append(0.0)

				else:
					
					# append current action from policy to csv for labview to open and implement
					labview_actions_list.append(actions[i][0] * 0.001)

				# check if implementing action for ramp upper 
				if (actor_states[i][1] + int(round(actions[i][1])) < RAMP_LOWER_BOUND) or (actor_states[i][1] + int(round(actions[i][1])) > RAMP_UPPER_BOUND):
					
					# append action of previous ramp upper to csv for labview to open and implement if new ramp upper exceeds bound 
					labview_actions_list.append(0)

				else:
					
					# append current ramp upper action from policy to csv for labview to open and implement
					labview_actions_list.append(int(round(actions[i][1])))

				# check if implementing action for ramp lower 
				if (actor_states[i][2] + int(round(actions[i][2])) > - RAMP_LOWER_BOUND) or (actor_states[i][2] + int(round(actions[i][2])) < - RAMP_UPPER_BOUND):
					
					# append action of previous ramp lower to csv for labview to open and implement if new ramp upper exceeds bound 
					labview_actions_list.append(0)

				else:
					
					# append current ramp lower action from policy to csv for labview to open and implement
					labview_actions_list.append(int(round(actions[i][2])))
				
				# save labview_actions
				np.savetxt(fname = LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(ep_time_step) + "_action.csv", 
						   X = np.expand_dims(np.array(labview_actions_list), axis = 1), delimiter = ',', fmt = '%1.5f')

			# check if previous action exists

			# update world ep_time_step
			ep_time_step += 1

			# boolean to track if data is obtained from disk
			is_data_obtained = False

			# repeatedly attempt to obtain data
			while not is_data_obtained:

				# # debugging messages
				# print(f"Fetching state from episode {eps} time step {ep_time_step} ... ")

				try:

					# open file
					actor_states_prime = np.loadtxt(fname = LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(ep_time_step) + "_state.csv", delimiter = ",")

					# make sure actor states prime is not empty
					if len(actor_states_prime) != STATE_DIMENSIONS:
						
						# raise exception with debugging messages
						raise ValueError(f"State from episode {eps} time step {ep_time_step} is empty / wrong length ... ")  
		
					# obtain initial state from labview
					actor_states_prime = np.expand_dims(actor_states_prime, axis = 0)

				except KeyboardInterrupt:
				
					# abort
					sys.exit()

				except:

					pass

				else: 

					# debugging messages
					print(f"Fetched state from episode {eps} time step {ep_time_step} ... ")
 
					# update boolean variable to exit while loop
					is_data_obtained = True
			
			# generate list for rewards, terminates, terminal conditions
			rewards = np.zeros(NUMBER_OF_AGENTS)
			terminates = np.full(NUMBER_OF_AGENTS, True)
			terminal_con = np.zeros(NUMBER_OF_AGENTS)

			# iterate over the number of agents
			for i in range(NUMBER_OF_AGENTS):

				# update rewards, terminates and terminal_con
				rewards[i], wf_state = reward(state = copy.deepcopy(actor_states_prime[i]), gt_state = goal, waveform_prominence = WAVEFORM_PROMINENCE, number_of_peaks_threshold = NUMBER_OF_PEAKS_THRESHOLD, 
											  norm_goal = norm_goal, reward_multiplier_constant = REWARD_MULTIPLIER_CONSTANT)
				terminates[i] = is_terminating(curr_time_step = ep_time_step, terminal_time_step = EPISODE_TIME_STEP_LIMIT)
				terminal_con[i] = terminating_condition(curr_state = copy.deepcopy(actor_states_prime[i]), gt_state = goal, curr_time_step = ep_time_step, terminal_time_step = EPISODE_TIME_STEP_LIMIT, 
														reward_threshold = REWARD_THRESHOLD, waveform_prominence = WAVEFORM_PROMINENCE, number_of_peaks_threshold = NUMBER_OF_PEAKS_THRESHOLD, 
														norm_goal = norm_goal, reward_multiplier_constant = REWARD_MULTIPLIER_CONSTANT)

				# add to sum of rewards
				sum_reward += rewards[i]

				# update terminal flag and terminal condition
				if is_terminal == False and terminates[i] == True:

					is_terminal = True
					terminal_condition = terminal_con[i]

			# reward debug message
			print(f"Total reward from episode {eps} time step {ep_time_step}: {rewards.sum()} ")

			# plt.subplot(1, 2, 1)
			# plt.plot(goal)
			# plt.plot(wf_state)

			# plt.subplot(1, 2, 2)
			# goal_prime = np.loadtxt(fname = GOAL_PATH, delimiter = ",")
			# goal_prime = goal_prime - goal_prime.mean()
			# # goal_prime[abs(goal_prime) <= WAVEFORM_THRESHOLD] = 0
			# # goal_prime = dynamic_peak_finder(goal_prime)
			# # goal_prime = abs(fft(goal_prime))
			# actor_states_plot = actor_states_prime[0][1:] - actor_states_prime[0][1:].mean()
			# # actor_states_plot[abs(actor_states_plot) <= WAVEFORM_THRESHOLD] = 0
			# # actor_states_plot = dynamic_peak_finder(actor_states_plot)
			# # actor_states_plot = abs(fft(actor_states_plot))
			# plt.plot(goal_prime)
			# plt.plot(actor_states_plot)
			# plt.show()

			# for maddpgv2_mlp agent drones to store memory in replay buffer 
			if MODEL == "maddpgv2_mlp" :

				# check if agent is training
				if MODE != "test":

					# obtain agent_critic_states and agent_critic_states_prime 
					# critic_states = np.concatenate((actor_states, actor_goals), axis = -1)
					# critic_states_prime = np.concatenate((actor_states_prime, actor_goals), axis = -1)
					critic_states = actor_states
					critic_states_prime = actor_states_prime

					# store states and actions in replay buffer
					maddpgv2_mlp_agents.replay_buffer.log(actor_state = actor_states, actor_state_prime = actor_states_prime, org_actor_goals = actor_goals, critic_state = critic_states, 
														  critic_state_prime = critic_states_prime, org_critic_goals = critic_goals, action = actions, org_rewards = rewards, 
														  is_done = terminates)

					# train agent models and obtain metrics for each agent for logging
					actor_loss_list, critic_loss_list, actor_grad_norm_list, critic_grad_norm_list, actor_learning_rate_list, critic_learning_rate_list = \
					maddpgv2_mlp_agents.apply_gradients_maddpgv2_mlp(num_of_agents = NUMBER_OF_AGENTS)

				else: 

					actor_loss_list, critic_loss_list, actor_grad_norm_list, critic_grad_norm_list, actor_learning_rate_list, critic_learning_rate_list = np.nan, np.nan, np.nan, np.nan, np.nan, np.nan

			# for maddpgv2_lstm agent drones to store memory in replay buffer 
			elif MODEL == "maddpgv2_lstm":

				# check if agent is training
				if MODE != "test":

					# obtain agent_critic_states and agent_critic_states_prime 
					# critic_states = np.concatenate((actor_states, actor_goals), axis = -1)
					# critic_states_prime = np.concatenate((actor_states_prime, actor_goals), axis = -1)
					critic_states = actor_states
					critic_states_prime = actor_states_prime

					# store states and actions in replay buffer
					maddpgv2_lstm_agents.replay_buffer.log(actor_state = actor_states, actor_state_prime = actor_states_prime, org_actor_goals = actor_goals, critic_state = critic_states, 
														   critic_state_prime = critic_states_prime, org_critic_goals = critic_goals, action = actions, org_rewards = rewards, 
														   is_done = terminates)

					# train agent models and obtain metrics for each agent for logging
					actor_loss_list, critic_loss_list, actor_grad_norm_list, critic_grad_norm_list, actor_learning_rate_list, critic_learning_rate_list = \
					maddpgv2_lstm_agents.apply_gradients_maddpgv2_lstm(num_of_agents = NUMBER_OF_AGENTS)

				else: 

					actor_loss_list, critic_loss_list, actor_grad_norm_list, critic_grad_norm_list, actor_learning_rate_list, critic_learning_rate_list = np.nan, np.nan, np.nan, np.nan, np.nan, np.nan

			# check if agent is training
			if MODE != "test":

				# populate addtional replay buffer for agent maddpgv2_mlp:
				if MODEL == "maddpgv2_mlp" and maddpgv2_mlp_agents.replay_buffer.org_replay_buffer.is_ep_terminal == True and MADDPGV2_MLP_ADDITIONAL_GOALS != 0:

					# populate her replay buffer
					maddpgv2_mlp_agents.replay_buffer.generate_her_replay_buffer()
				
				# populate addtional replay buffer for agent maddpgv2_lstm:
				elif MODEL == "maddpgv2_lstm" and maddpgv2_lstm_agents.replay_buffer.org_replay_buffer.is_ep_terminal == True and MADDPGV2_LSTM_ADDITIONAL_GOALS != 0:

					# populate her replay buffer
					maddpgv2_lstm_agents.replay_buffer.generate_her_replay_buffer()

			# metrics logging for agent

			# iterate over num of agent drones
			for i in range(NUMBER_OF_AGENTS): 

				# check if list is not nan and agent model is training
				if np.any(np.isnan(actor_loss_list)) == False and MODE != "test":

					# update sums
					sum_actor_loss += actor_loss_list[i]

				# check if list is not nan and agent model is training
				if np.any(np.isnan(critic_loss_list)) == False and MODE != "test":

					# update sums
					sum_critic_loss += critic_loss_list[i]

				# check if list is not nan and agent model is training
				if np.any(np.isnan(actor_grad_norm_list)) == False and MODE != "test":

					# update sums
					sum_actor_grad_norm += actor_grad_norm_list[i]

				# check if list is not nan and agent model is training
				if np.any(np.isnan(critic_grad_norm_list)) == False and MODE != "test":

					# update sums
					sum_critic_grad_norm += critic_grad_norm_list[i]
				
				# check if list is not nan and agent model is training
				if np.any(np.isnan(actor_learning_rate_list)) == False and MODE != "test":

					# update sums
					sum_actor_learning_rate += actor_learning_rate_list[i]
				
				# check if list is not nan and agent model is training
				if np.any(np.isnan(critic_learning_rate_list)) == False and MODE != "test":

					# update sums
					sum_critic_learning_rate += critic_learning_rate_list[i]

			# log wins
			if terminal_condition == 1:

				# add sum of wins for adversarial drones
				sum_wins += 1

				# append sums to all lists
				sum_wins_list.append(sum_wins)

				# add opp win and sum of agent win
				writer.add_scalar(tag = "terminate_info/agent_win", scalar_value = 1, global_step = eps)
				writer.add_scalar(tag = "terminate_info/sum_wins", scalar_value = sum_wins, global_step = eps)

			# log loss
			elif terminal_condition == 2:

				# append sums to all lists
				sum_wins_list.append(sum_wins)

				# add opp win and sum of agent win
				writer.add_scalar(tag = "terminate_info/agent_win", scalar_value = 0, global_step = eps)
				writer.add_scalar(tag = "terminate_info/sum_wins", scalar_value = sum_wins, global_step = eps)

			# update actor_states, adver_actor_states, agent_actor_states
			actor_states = actor_states_prime

		# generate metric logs for agent

		# obtain avg actor and critic loss
		avg_actor_loss = sum_actor_loss / float(NUMBER_OF_AGENTS * ep_time_step)
		avg_critic_loss = sum_critic_loss / float(NUMBER_OF_AGENTS * ep_time_step)

		# obtain avg actor and critic grad norms
		avg_actor_grad_norm = sum_actor_grad_norm / float(NUMBER_OF_AGENTS * ep_time_step)
		avg_critic_grad_norm = sum_critic_grad_norm / float(NUMBER_OF_AGENTS * ep_time_step)

		# obtain avg actor and critic learning rate
		avg_actor_learning_rate = sum_actor_learning_rate / float(NUMBER_OF_AGENTS * ep_time_step)
		avg_critic_learning_rate = sum_critic_learning_rate / float(NUMBER_OF_AGENTS * ep_time_step)

		# obtain avg_reward
		avg_reward = sum_reward / float(NUMBER_OF_AGENTS * ep_time_step)

		# check if agent model is training
		if MODE != "test":

			# add avg actor and critic loss for agent 
			writer.add_scalar(tag = "avg_actor_loss", scalar_value = avg_actor_loss, global_step = eps)
			writer.add_scalar(tag = "avg_critic_loss", scalar_value = avg_critic_loss, global_step = eps)

			# add avg actor and critic grad norms for agent
			writer.add_scalar(tag = "avg_actor_grad_norm", scalar_value = avg_actor_grad_norm, global_step = eps)
			writer.add_scalar(tag = "avg_critic_grad_norm", scalar_value = avg_critic_grad_norm, global_step = eps)

			# add avg actor and critic learning rate for agent
			writer.add_scalar(tag = "avg_actor_learning_rate", scalar_value = avg_actor_learning_rate, global_step = eps)
			writer.add_scalar(tag = "avg_critic_learning_rate", scalar_value = avg_critic_learning_rate, global_step = eps)

		# add avg_reward
		writer.add_scalar(tag = "avg_reward", scalar_value = avg_reward, global_step = eps)

		# append avg_actor_loss and avg_critic_loss to their respective list
		avg_actor_loss_list.append(avg_actor_loss)
		avg_critic_loss_list.append(avg_critic_loss)

		# append avg actor and critic grad norms to their respective list
		avg_actor_grad_norm_list.append(avg_actor_grad_norm)
		avg_critic_grad_norm_list.append(avg_critic_grad_norm)

		# append avg actor and critic learning rate to their respective list
		avg_actor_learning_rate_list.append(avg_actor_learning_rate)
		avg_critic_learning_rate_list.append(avg_critic_learning_rate)

		# append avg_reward to avg_reward_list
		avg_reward_list.append(avg_reward)

		# check if metrics is to be saved in csv log
		if SAVE_CSV_LOG == True:

			# generate pandas dataframe to store logs
			df = pd.DataFrame(list(zip(list(range(1, eps + 1)), sum_wins_list, avg_actor_loss_list, avg_critic_loss_list, avg_actor_grad_norm_list, avg_critic_grad_norm_list, avg_actor_learning_rate_list, 
							  				avg_critic_learning_rate_list, avg_reward_list)), 
							  columns = ['episodes', 'sum_wins', 'avg_actor_loss', 'avg_critic_loss', 'avg_actor_grad_norm', 'avg_critic_grad_norm', 'avg_actor_learning_rate', 'avg_critic_learning_rate', 
							  			 'avg_reward'])

			# store training logs
			df.to_csv(CSV_LOG_DIRECTORY + '/' + TRAINING_NAME + "_" + MODE + "_logs.csv", index = False)

		# check if agent is training and at correct episode to save
		if MODE != "test" and eps % SAVE_MODEL_RATE == 0:

			# check if agent model is maddpgv2_mlp
			if MODEL == "maddpgv2_mlp":

				# save all models
				maddpgv2_mlp_agents.save_all_models()

			# check if agent model is maddpgv2_mlp
			elif MODEL == "maddpgv2_lstm":

				# save all models
				maddpgv2_lstm_agents.save_all_models()
			
if __name__ == "__main__":

	print("Episodes: " + str(NUMBER_OF_EPISODES))
	print("Timesteps: " + str(EPISODE_TIME_STEP_LIMIT))
	print("\n")

	train_test(sys.argv[1])