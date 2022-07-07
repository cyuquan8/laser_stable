import numpy as np
import time
from scipy.fft import fft
from scipy.special import softmax
from scipy.signal import savgol_filter, find_peaks
import matplotlib.pyplot as plt

x = np.loadtxt(fname = "C:\\Users\\Admin\\Documents\\Experimental Control\\Testing and Development\\2022-05 - AI lockbox\\Neural Network Laser Locking\\Scripts\\goal.csv", delimiter=',')

t_1 = time.time()

x = x - x.mean()

x_unique = np.unique(x)

sorted_indices = np.argsort(x_unique)

x[abs(x) <= 0.15] = 0

if len(sorted_indices) == 1:

    ind_max_1, ind_max_2, ind_min_1, ind_min_2 = sorted_indices[0], sorted_indices[0], sorted_indices[0], sorted_indices[0] 

else:

    ind_max_1, ind_max_2, ind_min_1, ind_min_2 = sorted_indices[-1], sorted_indices[-2], sorted_indices[0], sorted_indices[1]
    mid_max_1_2 = (x_unique[ind_max_1] + x_unique[ind_max_2]) / 2
    mid_min_1_2 = (x_unique[ind_min_1] + x_unique[ind_min_2]) / 2

for i in range(len(x)):

    if ((x[i] > 0) and (x[i] <= mid_max_1_2)) or ((x[i] < 0) and (x[i] >= mid_min_1_2)):

        x[i] = 0

# #x_p = savgol_filter(x, 51, 3)
fft_goal = abs(fft(x))
y = softmax(fft_goal)


t_2 = time.time()

plt.plot(y) 
plt.show()
print(y)
print(y.sum())
print(t_2 - t_1)

