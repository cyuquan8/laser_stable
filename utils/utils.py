# ==========================================================================================================================================================
# utils
# purpose: utility functions
# ==========================================================================================================================================================

import numpy as np

def reward(state, gt_state):

    """ reward function for laser_stable environment """

    rew = 0

    # iterate over range of datapoints
    for index in range(len(state)):

        # obtain mse error
        rew -= (state[index] - gt_state[index])**2

    return rew