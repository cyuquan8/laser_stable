# ==========================================================================================================================================================
# utils
# purpose: utility functions
# ==========================================================================================================================================================

import os
import shutil
import numpy as np


NUMBER_OF_EPISODES = 10
EPISODE_TIME_STEP_LIMIT = 5

def reward(state, gt_state):

    """ function that generates reward for laser_stable environment """

    rew = 0

    # iterate over range of datapoints
    for index in range(len(state)):

        # obtain mse error
        rew -= (state[index] - gt_state[index])**2

    return rew

def is_terminal(curr_time_step, terminal_time_step):

    """ function that generates boolean terminal flag for laser_stable environment """

    return True if curr_time_step >= terminal_time_step else False

def terminating_condition(curr_state, gt_state, curr_time_step, terminal_time_step, error_threshold):

    """ function that returns terminating condition for laser_stable environment """
    """ 0 for in progress, 1 for success, 2 for fail """

    # check if episode has terminated
    if is_terminal(curr_time_step, terminal_time_step) == False:

        # return in progress
        return 0

    else:

        # check if absolute value of reward exceeds error threshold
        if abs(reward(curr_state, gt_state)) > error_threshold:

            # return fail
            return 2

        else:

            # return succeed
            return 1

def labview_read_action(labview_log_directory, episode, episode_time_step):

    """ function for labiew to read actions """

    # obtain initial state from labview
    actions = np.loadtxt(fname = labview_log_directory + "\\episode_" + str(episode) + "_time_step_" + str(episode_time_step) + "_action.csv", delimiter = ",")

    # check if previous state exists
    if os.path.exists(labview_log_directory + "\\episode_" + str(episode) + "_time_step_" + str(episode_time_step - 1) + "_action.csv") == True:

        # remove previous csv file for action
        os.remove(labview_log_directory + "\\episode_" + str(episode) + "_time_step_" + str(episode_time_step - 1) + "_action.csv")


    return actions[0]

def labview_write_state(state, labview_log_directory, episode, episode_time_step):

    """ function for labiew to write state """

    # obtain initial state from labview
    np.savetxt(fname = labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step) + "_state.csv", X = state, delimiter = ',')


    # check if previous state exists
    if os.path.exists(labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step - 1) + "_state.csv") == True:

        # remove previous csv file for state
        os.remove(labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step - 1) + "_state.csv")


    return 'Written'

def how_many_episodes():
	return NUMBER_OF_EPISODES

def how_many_timesteps():
	return EPISODE_TIME_STEP_LIMIT