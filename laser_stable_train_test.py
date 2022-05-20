# ==========================================================================================================================================================
# main train test
# purpose: main training function that interacts with environment through saved files
# ==========================================================================================================================================================

import os
import shutil
import numpy as np
import torch as T
import pandas as pd
from maddpgv2_mlp.maddpgv2_mlp import maddpgv2_mlp
from utils.utils import reward, is_terminal, terminating_condition

# general options
SCENARIO_NAME												= "zone_def_tag" 
MODEL 														= "maddpgv2_mlp" 
MODE 														= "train"
TRAINING_NAME 												= SCENARIO_NAME + MODEL + "_500_time_steps"
CSV_LOG_DIRECTORY											= "csv_log" + '/' + SCENARIO_NAME
LABVIEW_LOG_DIRECTORY										= "labview_log" +  '/' + SCENARIO_NAME
GOAL_PATH													= "goal.csv"
NUMBER_OF_EPISODES 											= 100000
EPISODE_TIME_STEP_LIMIT										= 500
SAVE_MODEL_RATE 											= 100
SAVE_CSV_LOG												= True

# env options
NUMBER_OF_AGENTS 											= 1
GOAL_DIMENSIONS												= 2000
STATE_DIMENSIONS 											= GOAL_DIMENSIONS + 1
ACTION_DIMENSIONS  											= 1
ACTION_NOISE												= 1
ACTION_RANGE												= 1
ERROR_THRESHOLD												= 0.1

# define hyperparameters for maddpgv2_mlp
MADDPGV2_MLP_DISCOUNT_RATE 									= 0.99
MADDPGV2_MLP_LEARNING_RATE_ACTOR 							= 0.0005
MADDPGV2_MLP_LEARNING_RATE_CRITIC 							= 0.0005
MADDPGV2_MLP_ACTOR_DROPOUT									= 0
MADDPGV2_MLP_CRITIC_DROPOUT									= 0
MADDPGV2_MLP_TAU 											= 0.01	  
MADDPGV2_MLP_MEMORY_SIZE 									= 100000
MADDPGV2_MLP_BATCH_SIZE 									= 128
MADDPGV2_MLP_UPDATE_TARGET 									= None
MADDPGV2_MLP_GRADIENT_CLIPPING								= True
MADDPGV2_MLP_GRADIENT_NORM_CLIP								= 1
MADDPGV2_MLP_GOAL 											= np.loadtxt(fname = GOAL_PATH, delimiter = ",")
MADDPGV2_MLP_ADDITIONAL_GOALS								= 4
MADDPGV2_MLP_GOAL_STRATEGY									= "future"

MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS						= [512, 256, 128]
MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS							= [512, 256, 128]

def train_test():

	""" function to execute experiments to train or test models based on different algorithms """

	# check model
	if MODEL == "maddpgv2_mlp":

		maddpgv2_mlp_agents = maddpgv2_mlp(mode = MODE, scenario_name = SCENARIO_NAME, training_name = TRAINING_NAME, discount_rate = MADDPGV2_MLP_DISCOUNT_RATE, 
										   lr_actor = MADDPGV2_MLP_LEARNING_RATE_ACTOR, lr_critic = MADDPGV2_MLP_LEARNING_RATE_CRITIC, num_agents = NUMBER_OF_AGENTS, 
										   actor_dropout_p = MADDPGV2_MLP_ACTOR_DROPOUT, critic_dropout_p = MADDPGV2_MLP_CRITIC_DROPOUT, 
										   state_fc_input_dims = [STATE_DIMENSIONS for i in range(NUMBER_OF_AGENTS)], actor_state_fc_output_dims = MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS, 
										   critic_state_fc_output_dims = MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS, action_dims = ACTION_DIMENSIONS, 
										   goal_fc_input_dims = [GOAL_DIMENSIONS for i in range(NUMBER_OF_AGENTS)], tau = MADDPGV2_MLP_TAU, actor_action_noise = ACTION_NOISE, 
										   actor_action_range = ACTION_RANGE, mem_size = MADDPGV2_MLP_MEMORY_SIZE, batch_size = MADDPGV2_MLP_BATCH_SIZE, update_target = MADDPGV2_MLP_UPDATE_TARGET, 
										   grad_clipping = MADDPGV2_MLP_GRADIENT_CLIPPING, grad_norm_clip = MADDPGV2_MLP_GRADIENT_NORM_CLIP, num_of_add_goals = MADDPGV2_MLP_ADDITIONAL_GOALS, 
										   goal_strategy = MADDPGV2_MLP_GOAL_STRATEGY)

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
	avg_reward_list = []
	win_time_step_list = []

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

			try:

				# obtain initial state from labview
				actor_states = np.expand_dims(np.loadtxt(fname = LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(ep_time_step) + "_state", delimiter = ","), axis = 0)

			except:

				pass

			else: 

				# update boolean variable to exit while loop
				is_data_obtained = True

		# set up agent actor goals for maddpg_gnn and mappo_gnn
		if MODEL == "maddpgv2_mlp":
			
			goal = MADDPGV2_MLP_GOAL
			actor_goals = np.array([[goal] for i in range(NUMBER_OF_AGENTS)])
			critic_goals = np.array([[goal] for i in range(NUMBER_OF_AGENTS)]).reshape(1, -1)
		
		# variables to track metrics
		sum_actor_loss = 0
		sum_critic_loss = 0
		sum_actor_grad_norm = 0
		sum_critic_grad_norm = 0
		sum_reward = 0

		# iterate till episode terminates
		while is_terminal == 0:
				
			# obtain actions for agent_maddpgv2_mlp_agents  
			if MODEL == "maddpgv2_mlp":
				
				# actor_states concatenated with actor_goals
				actor_states_p_goal = np.concatenate((actor_states, actor_goals), axis = -1)

				# obtain motor and communication actions for agent drones
				# mode is always 'test' as the environment handles the addition of noise to the actions
				actions = maddpgv2_mlp_agents.select_actions(mode = "test", actor_state_list = agent_actor_states_p_goal)

			# save action from policy to csv for labview to open and implement
			np.savetxt(fname = LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(ep_time_step) + "_action", X = actions, delimiter = ',')

			# check if previous action exists
			if os.path.exists(LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(ep_time_step - 1) + "_action") == True:

				# remove previous csv file for action
				os.remove(LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(time_step - 1) + "_action")

			# update world ep_time_step
			ep_time_step += 1

			# boolean to track if data is obtained from disk
			is_data_obtained = False

			# repeatedly attempt to obtain data
			while not is_data_obtained:

				try:

					# obtain initial state from labview
					actor_states_prime = np.expand_dims(np.loadtxt(fname = LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(ep_time_step) + "_state", delimiter = ","), axis = 0)

				except:

					pass

				else: 

					# check if previous state exists
					if os.path.exists(LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(ep_time_step - 1) + "_state") == True:

						# remove previous csv file for action
						os.remove(LABVIEW_LOG_DIRECTORY + "/episode_" + str(eps) + "_time_step_" + str(time_step - 1) + "_state")

					# update boolean variable to exit while loop
					is_data_obtained = True
			
			# generate list for rewards, terminates, terminal conditions
			rewards = np.zeros(NUMBER_OF_AGENTS)
			terminates = np.full(NUMBER_OF_AGENTS, True)
			terminal_con = np.zeros(NUMBER_OF_AGENTS)

			# iterate over the number of agents
			for i in range(NUMBER_OF_AGENTS):

				# update rewards, terminates and terminal_con
				rewards[i] = reward(state = actor_states_prime[i], gt_state = goal)
				terminates[i] = is_terminal(curr_time_step = ep_time_step, terminal_time_step = EPISODE_TIME_STEP_LIMIT)
				terminal_con[i] = terminating_condition(curr_state = actor_states_prime[i], gt_state = goal, curr_time_step = ep_time_step, terminal_time_step = EPISODE_TIME_STEP_LIMIT, 
														error_threshold = ERROR_THRESHOLD)

				# add to sum of rewards
				sum_reward += rewards[i]

				# update terminal flag and terminal condition
				if is_terminal == False and terminates[i] == True:

					is_terminal = True
					terminal_condition = terminal_con[i]

			# for maddpgv2_mlp agent drones to store memory in replay buffer 
			if MODEL == "maddpgv2_mlp" :

				# check if agent is training
				if MODE != "test":

					# obtain agent_critic_states and agent_critic_states_prime 
					critic_states = np.concatenate((actor_states, actor_goals), axis = -1)
					critic_states_prime = np.concatenate((actor_states_prime, actor_goals), axis = -1)

					# store states and actions in replay buffer
					maddpgv2_mlp_agents.replay_buffer.log(actor_state = actor_states, actor_state_prime = actor_states_prime, org_actor_goals = actor_goals, critic_state = critic_states, 
														  critic_state_prime = critic_states_prime, org_critic_goals = critic_goals, action = actions, org_rewards = rewards, 
														  is_done = terminates)

					# train agent models and obtain metrics for each agent for logging
					actor_loss_list, critic_loss_list, actor_grad_norm_list, critic_grad_norm_list = maddpgv2_mlp_agents.apply_gradients_maddpgv2_mlp(num_of_agents = NUMBER_OF_AGENTS)

				else: 

					actor_loss_list, critic_loss_list, actor_grad_norm_list, critic_grad_norm_list = np.nan, np.nan, np.nan, np.nan

			# check if agent is training
			if MODE != "test":

				# populate addtional replay buffer for agent maddpgv2_gnn:
				if MODEL == "maddpgv2_mlp" and maddpgv2_mlp_agents.replay_buffer.org_replay_buffer.is_ep_terminal == True and MADDPGV2_MLP_ADDITIONAL_GOALS != 0:

					# populate her replay buffer
					maddpgv2_mlp_agents.replay_buffer.generate_her_replay_buffer()

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
				writer.add_scalar(tag = "terminate_info/agent_drone_win", scalar_value = 0, global_step = eps)
				writer.add_scalar(tag = "terminate_info/sum_wins", scalar_value = sum_wins, global_step = eps)

			# update actor_states, adver_actor_states, agent_actor_states
			actor_states = actor_states_prime

		# generate metric logs for agent

		# obtain avg actor and critic loss
		avg_actor_loss = sum_agent_actor_loss / float(NUMBER_OF_AGENTS * ep_time_step)
		avg_critic_loss = sum_agent_critic_loss / float(NUMBER_OF_AGENTS * ep_time_step)

		# obtain avg actor and critic grad norms
		avg_actor_grad_norm = sum_actor_grad_norm / float(NUMBER_OF_AGENTS * ep_time_step)
		avg_critic_grad_norm = sum_critic_grad_norm / float(NUMBER_OF_AGENTS * ep_time_step)

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

		# add avg_reward
		writer.add_scalar(tag = "avg_reward", scalar_value = avg_reward, global_step = eps)

		# append avg_actor_loss and avg_critic_loss to their respective list
		avg_actor_loss_list.append(avg_actor_loss)
		avg_critic_loss_list.append(avg_critic_loss)

		# append avg actor and critic grad norms to their respective list
		avg_actor_grad_norm_list.append(avg_actor_grad_norm)
		avg_critic_grad_norm_list.append(avg_critic_grad_norm)

		# append avg_reward to avg_reward_list
		avg_reward_list.append(avg_reward)

		# check if metrics is to be saved in csv log
		if SAVE_CSV_LOG == True:

			# generate pandas dataframe to store logs
			df = pd.DataFrame(list(zip(list(range(1, eps + 1)), sum_wins_list, avg_actor_loss_list, avg_critic_loss_list, avg_actor_grad_norm_list, avg_critic_grad_norm_list, avg_reward_list)), 
							  columns = ['episodes', 'sum_wins', 'avg_actor_loss', 'avg_critic_loss', 'avg_actor_grad_norm', 'avg_critic_grad_norm', 'avg_reward'])

			# store training logs
			df.to_csv(CSV_LOG_DIRECTORY + '/' + GENERAL_TRAINING_NAME + "_" + MODE + "_logs.csv", index = False)

		# check if agent is training and at correct episode to save
		if MODE != "test" and eps % SAVE_MODEL_RATE == 0:

			# check if agent model is maddpgv2_gnn
			if MODEL == "maddpgv2_mlp":

				# save all models
				maddpgv2_mlp_agents.save_all_models()

if __name__ == "__main__":

	train_test()