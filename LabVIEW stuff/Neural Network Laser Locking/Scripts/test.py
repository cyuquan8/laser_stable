import numpy as np

episode_number = np.loadtxt(fname=".\\config.txt",delimiter=',')[0]

print(int(episode_number))