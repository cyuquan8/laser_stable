import time
import numpy as np

SCENARIO_NAME												= "rb_laser_locking" 
LABVIEW_LOG_DIRECTORY										= "labview_log" +  "\\" + SCENARIO_NAME
NUMBER_OF_EPISODES 											= 3
EPISODE_TIME_STEP_LIMIT										= 2

wait_time = 2
result = []


def train_test():
    print("File directory: " + str(LABVIEW_LOG_DIRECTORY))
    for jj in range(NUMBER_OF_EPISODES):
        for kk in range(EPISODE_TIME_STEP_LIMIT):
            file_obtained = False
            try_counter = 0
            while not file_obtained:
                path = LABVIEW_LOG_DIRECTORY + "\\episode_" + str(jj) + "_time_step_" + str(kk) + "_state"
                print("Trying to load file " + path)
                try:
                    file = np.array(np.loadtxt(fname = path))
                    result.append(file)
                    file_obtained = True
                    try_counter += 1
                    print("Successfully loaded " + path)
                    np.savetxt(fname = LABVIEW_LOG_DIRECTORY + "\\episode_" + str(jj) + "_time_step_" + str(kk) + "_action", X = file)
                    time.sleep(wait_time)
                except:
                    try_counter += 1
                    print("Failed to load " + path + ", retrying...")
                    time.sleep(wait_time)
                    pass
    return result

print(train_test())