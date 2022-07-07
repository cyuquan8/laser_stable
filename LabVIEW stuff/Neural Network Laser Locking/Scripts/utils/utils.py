# ==========================================================================================================================================================
# utils
# purpose: utility functions
# ==========================================================================================================================================================

import os
import shutil
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft
from scipy.special import softmax
from matplotlib.lines import Line2D

def dynamic_peak_finder(x):

    """ function to isolate the maximum positive and negative peaks """

    # remove duplicates
    x_unique = np.unique(x)

    # sorted in ascending order
    sorted_indices = np.argsort(x_unique)

    # obtain top 2 max and min peaks
    if len(sorted_indices) == 1:

        ind_max_1, ind_max_2, ind_min_1, ind_min_2 = sorted_indices[0], sorted_indices[0], sorted_indices[0], sorted_indices[0] 

    else:

        ind_max_1, ind_max_2, ind_min_1, ind_min_2 = sorted_indices[-1], sorted_indices[-2], sorted_indices[0], sorted_indices[1]
        mid_max_1_2 = (x_unique[ind_max_1] + x_unique[ind_max_2]) / 2
        mid_min_1_2 = (x_unique[ind_min_1] + x_unique[ind_min_2]) / 2

    # thresholding
    for i in range(len(x)):

        if ((x[i] > 0) and (x[i] <= mid_max_1_2)) or ((x[i] < 0) and (x[i] >= mid_min_1_2)):

            x[i] = 0
    
    return x

def reward(state, gt_state, waveform_threshold):

    """ function that generates reward for laser_stable environment """
    """ note that gt_state is already transformed """

    rew = 0

    # obtain fft, threshold, softmaxed waveform state
    wf_state = state[1:]
    wf_state = wf_state - wf_state.mean()
    wf_state[abs(wf_state) <= waveform_threshold] = 0
    wf_state = dynamic_peak_finder(wf_state)

    # correlate
    corr_auto = np.correlate(wf_state, gt_state, "full")

    # cal reward
    rew += np.sum(np.absolute(corr_auto))

    return rew, wf_state

def is_terminating(curr_time_step, terminal_time_step):

    """ function that generates boolean terminal flag for laser_stable environment """

    return True if curr_time_step >= (terminal_time_step - 1) else False

def terminating_condition(curr_state, gt_state, curr_time_step, terminal_time_step, error_threshold, waveform_threshold):

    """ function that returns terminating condition for laser_stable environment """
    """ 0 for in progress, 1 for success, 2 for fail """

    # check if episode has terminated
    if is_terminating(curr_time_step, terminal_time_step) == False:

        # return in progress
        return 0

    else:

        # check if absolute value of reward exceeds error threshold
        if abs(reward(curr_state, gt_state, waveform_threshold)[0]) > error_threshold:

            # return succeed
            return 1

        else:

            # return fail
            return 2

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

def plot_grad_flow(named_parameters):

    '''Plots the gradients flowing through different layers in the net during training.
    Can be used for checking for possible gradient vanishing / exploding problems.
    
    Usage: Plug this function in Trainer class after loss.backwards() as 
    "plot_grad_flow(self.model.named_parameters())" to visualize the gradient flow'''

    ave_grads = []
    max_grads= []
    layers = []

    for n, p in named_parameters:

        if(p.requires_grad) and ("bias" not in n):
            
            layers.append(n)
            ave_grads.append(p.grad.abs().mean())
            max_grads.append(p.grad.abs().max())

            print(f"Parameter {n}")
            print(f"Parameter {n} average gradient: {p.grad.abs().mean()}")
            print(f"Parameter {n} max gradient: {p.grad.abs().mean()}")

    plt.bar(np.arange(len(max_grads)), max_grads, alpha=0.1, lw=1, color="c")
    plt.bar(np.arange(len(max_grads)), ave_grads, alpha=0.1, lw=1, color="b")
    plt.hlines(0, 0, len(ave_grads)+1, lw=2, color="k" )
    plt.xticks(range(0,len(ave_grads), 1), layers, rotation="vertical")
    plt.xlim(left=0, right=len(ave_grads))
    plt.ylim(bottom = -0.001, top=0.02) # zoom in on the lower gradient regions
    plt.xlabel("Layers")
    plt.ylabel("Average Gradient")
    plt.title("Gradient Flow")
    plt.grid(True)
    plt.legend([Line2D([0], [0], color="c", lw=4),
                Line2D([0], [0], color="b", lw=4),
                Line2D([0], [0], color="k", lw=4)], ['max-gradient', 'mean-gradient', 'zero-gradient'])