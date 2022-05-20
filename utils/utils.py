# ==========================================================================================================================================================
# utils
# purpose: utility functions
# ==========================================================================================================================================================

import os
import shutil
import numpy as np

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

    # boolean to track if data is obtained from disk
    is_data_obtained = False

    # repeatedly attempt to obtain data
    while not is_data_obtained:

        try:

            # obtain initial state from labview
            actions = np.loadtxt(fname = labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step) + "_action", delimiter = ",")

        except:

            pass

        else: 

            # check if previous state exists
            if os.path.exists(labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step - 1) + "_action") == True:

                # remove previous csv file for action
                os.remove(labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step - 1) + "_action")

            # update boolean variable to exit while loop
            is_data_obtained = True

    return actions

def labview_write_state(state, labview_log_directory, episode, episode_time_step):

    """ function for labiew to write state """

    # boolean to track if data is obtained from disk
    is_data_obtained = False

    # repeatedly attempt to obtain data
    while not is_data_obtained:

        try:

            # obtain initial state from labview
            np.savetxt(fname = labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step) + "_state", X = state, delimiter = ',')

        except:

            pass

        else: 

            # check if previous state exists
            if os.path.exists(labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step - 1) + "_state") == True:

                # remove previous csv file for action
                os.remove(labview_log_directory + "/episode_" + str(episode) + "_time_step_" + str(episode_time_step - 1) + "_state")

            # update boolean variable to exit while loop
            is_data_obtained = True