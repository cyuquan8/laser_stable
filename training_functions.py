# ==========================================================================================================================================================
# training functions
# purpose: training functions to be imported labview
# ==========================================================================================================================================================

import json
import numpy as np
import torch as T
import pandas as pd
from maddpgv2_mlp.maddpgv2_mlp import maddpgv2_mlp
from utils.utils import reward

def initialise():

	""" function to import the modules being used for ml in labview """

	# general options
	SCENARIO_NAME												= "zone_def_tag" 
	MODEL 														= "maddpgv2_mlp" 
	MODE 														= "train"
	TRAINING_NAME 												= SCENARIO_NAME + MODEL + "_500_time_steps"
	CSV_LOG_DIRECTORY											= "csv_log" + '/' + SCENARIO_NAME
	NUMBER_OF_EPISODES 											= 100000
	EPISODE_TIME_STEP_LIMIT										= 500

	# env options
	NUMBER_OF_AGENTS 											= 1
	NUMBER_OF_STATE_DATAPOINTS									= 2000
	STATE_DIMENSIONS 											= NUMBER_OF_STATE_DATAPOINTS + 1
	ACTION_DIMENSIONS  											= 1
	ACTION_NOISE												= 1

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
	MADDPGV2_MLP_GOAL 											= None
	MADDPGV2_MLP_ADDITIONAL_GOALS								= 4
	MADDPGV2_MLP_GOAL_STRATEGY									= "future"

	MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS						= [512, 256, 128]
	MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS							= [512, 256, 128]

	# check model
	if MODEL == "maddpgv2_mlp":

		maddpgv2_mlp(mode = MODE, scenario_name = SCENARIO_NAME, training_name = TRAINING_NAME, discount_rate = MADDPGV2_MLP_DISCOUNT_RATE, lr_actor = MADDPGV2_MLP_LEARNING_RATE_ACTOR, 
					 lr_critic = MADDPGV2_MLP_LEARNING_RATE_CRITIC, num_agents = NUMBER_OF_AGENTS, actor_dropout_p = MADDPGV2_MLP_ACTOR_DROPOUT, critic_dropout_p = MADDPGV2_MLP_CRITIC_DROPOUT, 
					 state_fc_input_dims = [STATE_DIMENSIONS for i in range(NUMBER_OF_AGENTS)], actor_state_fc_output_dims = MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS, 
					 critic_state_fc_output_dims = MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS, action_dims = ACTION_DIMENSIONS, goal_fc_input_dims = [STATE_DIMENSIONS for i in range(NUMBER_OF_AGENTS)], 
					 tau = MADDPGV2_MLP_TAU, actor_action_noise = ACTION_NOISE, mem_size = MADDPGV2_MLP_MEMORY_SIZE, batch_size = MADDPGV2_MLP_BATCH_SIZE, update_target = MADDPGV2_MLP_UPDATE_TARGET, 
					 grad_clipping = MADDPGV2_MLP_GRADIENT_CLIPPING, grad_norm_clip = MADDPGV2_MLP_GRADIENT_NORM_CLIP, num_of_add_goals = MADDPGV2_MLP_ADDITIONAL_GOALS, 
					 goal_strategy = MADDPGV2_MLP_GOAL_STRATEGY)

	# dictionary of parameters
	param_dict = {

		# general options
		"SCENARIO_NAME"												: SCENARIO_NAME, 
		"MODEL" 													: MODEL, 
		"MODE" 														: MODE,
		"TRAINING_NAME" 											: TRAINING_NAME,
		"CSV_LOG_DIRECTORY"											: CSV_LOG_DIRECTORY,
		"NUMBER_OF_EPISODES" 										: NUMBER_OF_EPISODES,
		"EPISODE_TIME_STEP_LIMIT"									: EPISODE_TIME_STEP_LIMIT,

		# env options
		"NUMBER_OF_AGENTS" 											: NUMBER_OF_AGENTS,
		"NUMBER_OF_STATE_DATAPOINTS"								: NUMBER_OF_STATE_DATAPOINTS,
		"STATE_DIMENSIONS" 											: STATE_DIMENSIONS,
		"ACTION_DIMENSIONS"  										: ACTION_DIMENSIONS,
		"ACTION_NOISE"												: ACTION_NOISE,

		# define hyperparameters for maddpgv2_mlp
		"MADDPGV2_MLP_DISCOUNT_RATE" 								: MADDPGV2_MLP_DISCOUNT_RATE,
		"MADDPGV2_MLP_LEARNING_RATE_ACTOR" 							: MADDPGV2_MLP_LEARNING_RATE_ACTOR,
		"MADDPGV2_MLP_LEARNING_RATE_CRITIC" 						: MADDPGV2_MLP_ACTOR_DROPOUT,
		"MADDPGV2_MLP_ACTOR_DROPOUT"								: MADDPGV2_MLP_ACTOR_DROPOUT,
		"MADDPGV2_MLP_CRITIC_DROPOUT"								: MADDPGV2_MLP_CRITIC_DROPOUT,
		"MADDPGV2_MLP_TAU" 											: MADDPGV2_MLP_TAU,
		"MADDPGV2_MLP_MEMORY_SIZE" 									: MADDPGV2_MLP_MEMORY_SIZE,
		"MADDPGV2_MLP_BATCH_SIZE" 									: MADDPGV2_MLP_BATCH_SIZE,
		"MADDPGV2_MLP_UPDATE_TARGET" 								: MADDPGV2_MLP_UPDATE_TARGET,
		"MADDPGV2_MLP_GRADIENT_CLIPPING"							: MADDPGV2_MLP_GRADIENT_NORM_CLIP,
		"MADDPGV2_MLP_GRADIENT_NORM_CLIP"							: MADDPGV2_MLP_GRADIENT_NORM_CLIP,
		"MADDPGV2_MLP_GOAL" 										: MADDPGV2_MLP_GOAL,
		"MADDPGV2_MLP_ADDITIONAL_GOALS"								: MADDPGV2_MLP_ADDITIONAL_GOALS,
		"MADDPGV2_MLP_GOAL_STRATEGY"								: MADDPGV2_MLP_GOAL_STRATEGY,

		"MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS"						: MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS,
		"MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS"						: MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS

	}

	# dictionary for metrics
	metrics_dict{

		# variables to track the sum of win
		"SUM_OF_WINS" : 0

		# list to store metrics to be converted to csv for postprocessing
		"SUM_OF_WINS_LIST" 											: []
		"AVERAGE_ACTOR_LOSS_LIST" 									: []
		"AVERAGE_CRITIC_LOSS_LIST" 									: []
		"AVERAGE_ACTOR_GRAD_NORM_LIST" 								: []
		"AVERAGE_CRITIC_GRAD_NORM_LIST" 							: []
		"GOALS_LIST" 												: []

	}

	# generate replay buffer list to pass relay buffer down
	replay_buffer_list = []

	# check if training
	if MODE == "train" or "load_and_train":

		# iterate over number of replay buffers
		for i in range(MADDPGV2_MLP_ADDITIONAL_GOALS + 1):

			# check for original replay buffer
			if i == 0:

				replay_buffer_goal_dict = {

					"actor_state_log_list": maddpgv2_mlp.replay_buffer.org_replay_buffer.actor_state_log_list,
					"actor_state_prime_log_list": maddpgv2_mlp.replay_buffer.org_replay_buffer.actor_state_prime_log_list,
					"actor_action_log_list": maddpgv2_mlp.replay_buffer.org_replay_buffer.actor_action_log_list,
					"actor_goals_log_list": maddpgv2_mlp.replay_buffer.org_replay_buffer.actor_goals_log_list,
					"critic_state_log": maddpgv2_mlp.replay_buffer.org_replay_buffer.critic_state_log,
					"critic_state_prime_log": maddpgv2_mlp.replay_buffer.org_replay_buffer.critic_state_prime_log,
					"critic_goals_log": maddpgv2_mlp.replay_buffer.org_replay_buffer.critic_goals_log,
					"rewards_log": maddpgv2_mlp.replay_buffer.org_replay_buffer.rewards_log,
					"terminal_log": maddpgv2_mlp.replay_buffer.org_replay_buffer.terminal_log

				}

			# case of additional goals
			else:

				replay_buffer_goal_dict = {

					"actor_state_log_list": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].actor_state_log_list,
					"actor_state_prime_log_list": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].actor_state_prime_log_list,
					"actor_action_log_list": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].actor_action_log_list,
					"actor_goals_log_list": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].actor_goals_log_list,
					"critic_state_log": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].critic_state_log,
					"critic_state_prime_log": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].critic_state_prime_log,
					"critic_goals_log": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].critic_goals_log,
					"rewards_log": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].rewards_log,
					"terminal_log": maddpgv2_mlp.replay_buffer.add_goals_replay_buffer_list[i - 1].terminal_log

				}

			# append to list
			replay_buffer_list.append(replay_buffer_goal_dict)

	

	# save model
	maddpgv2_mlp.save_all_models

	return [json.dumps(param_dict), json.dumps(metrics_dict), json.dumps(replay_buffer_list)] 

def select_actions(param_dict_json, metrics_dict_json, replay_buffer_list_json):

	""" function to select actions """

	# load parameters dictionary and replay buffer list
	param_dict = json.loads(param_dict_json)

	# reinitialise model
	maddpgv2_mlp(mode = param_dict["MODE"], scenario_name = param_dict["SCENARIO_NAME"], training_name = param_dict["TRAINING_NAME"], discount_rate = param_dict["MADDPGV2_MLP_DISCOUNT_RATE"], 
				 lr_actor = param_dict["MADDPGV2_MLP_LEARNING_RATE_ACTOR"], lr_critic = param_dict["MADDPGV2_MLP_LEARNING_RATE_CRITIC"], num_agents = param_dict["NUMBER_OF_AGENTS"], 
				 actor_dropout_p = param_dict["MADDPGV2_MLP_ACTOR_DROPOUT"], critic_dropout_p = param_dict["MADDPGV2_MLP_CRITIC_DROPOUT"], 
				 state_fc_input_dims = [param_dict["STATE_DIMENSIONS"] for i in range(param_dict["NUMBER_OF_AGENTS"])], 
				 actor_state_fc_output_dims = param_dict["MADDPGV2_MLP_ACTOR_OUTPUT_DIMENSIONS"], critic_state_fc_output_dims = param_dict["MADDPGV2_MLP_CRITIC_FC_OUTPUT_DIMS"], 
				 action_dims = param_dict["ACTION_DIMENSIONS"], goal_fc_input_dims = [param_dict["STATE_DIMENSIONS"] for i in range(param_dict["NUMBER_OF_AGENTS"])], 
				 tau = param_dict["MADDPGV2_MLP_TAU"], actor_action_noise = param_dict["ACTION_NOISE"], mem_size = param_dict["MADDPGV2_MLP_MEMORY_SIZE"], 
				 batch_size = param_dict["MADDPGV2_MLP_BATCH_SIZE"], update_target = param_dict["MADDPGV2_MLP_UPDATE_TARGET"], grad_clipping = param_dict["MADDPGV2_MLP_GRADIENT_CLIPPING"], 
				 grad_norm_clip = param_dict["MADDPGV2_MLP_GRADIENT_NORM_CLIP"], num_of_add_goals = param_dict["MADDPGV2_MLP_ADDITIONAL_GOALS"], 
				 goal_strategy = param_dict["MADDPGV2_MLP_GOAL_STRATEGY"])

	# load saved model
	maddpgv2_mlp.load_all_models()

	# select actions 
	return maddpgv2_mlp.select_actions(state), param_dict_json, metrics_dict_json, replay_buffer_list_json

def step():