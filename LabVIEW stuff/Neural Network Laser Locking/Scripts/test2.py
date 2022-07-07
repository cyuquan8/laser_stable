import numpy as np 
import matplotlib.pyplot as plt
from utils.utils import dynamic_peak_finder

# import waveforms
goal = np.loadtxt(".\\goal.csv")
good = np.loadtxt(".\\test_good.csv")
kinda_good = np.loadtxt(".\\test_kinda_good.csv")
kinda_bad = np.loadtxt(".\\test_kinda_bad.csv")
bad = np.loadtxt(".\\test_bad.csv")

# compute norms
# norm_goal = np.linalg.norm(goal)
# norm_good = np.linalg.norm(good)
# norm_kinda_good = np.linalg.norm(kinda_good)
# norm_kinda_bad = np.linalg.norm(kinda_bad)
# norm_bad = np.linalg.norm(bad)

norm_goal = 1
norm_good = 1
norm_kinda_good = 1
norm_kinda_bad = 1
norm_bad = 1

# subtract mean
goal = goal - goal.mean()
good = good - good.mean()
kinda_good = kinda_good - kinda_good.mean()
kinda_bad = kinda_bad - kinda_bad.mean()
bad = bad - bad.mean()

# thresholding
goal[abs(goal) <= 0.2] = 0
good[abs(good) <= 0.2] = 0
kinda_good[abs(kinda_good) <= 0.2] = 0
kinda_bad[abs(kinda_bad) <= 0.2] = 0
bad[abs(bad) <= 0.2] = 0

# dpf
goal = dynamic_peak_finder(goal)
good = dynamic_peak_finder(good)
kinda_good = dynamic_peak_finder(kinda_good)
kinda_bad = dynamic_peak_finder(kinda_bad)
bad = dynamic_peak_finder(bad)

# compute correlation arrays
corr_auto = np.correlate(goal, goal, "full")/(norm_goal * norm_goal)
corr_good = np.correlate(goal, good, "full")[1:]/(norm_goal * norm_good)
corr_kinda_good = np.correlate(goal, kinda_good, "full") /(norm_goal * norm_kinda_good)
corr_bad = np.correlate(goal, bad, "full")/(norm_goal * norm_kinda_bad)
corr_kinda_bad = np.correlate(goal, kinda_bad, "full")/(norm_goal * norm_bad)

#
reward_goal = np.sum(np.absolute(corr_auto))
reward_good = np.sum(np.absolute(corr_good))
reward_kinda_good = np.sum(np.absolute(corr_kinda_good))
reward_kinda_bad = np.sum(np.absolute(corr_kinda_bad))
reward_bad = np.sum(np.absolute(corr_bad))

# plot
plt.subplot(5,2,1)
plt.plot(goal)
plt.subplot(5,2,2)
plt.plot(corr_auto)

plt.subplot(5,2,3)
plt.plot(good, color = 'green')
plt.subplot(5, 2, 4)
plt.plot(corr_good, color = 'green')

plt.subplot(5,2,5)
plt.plot(kinda_good, color = 'lightgreen')
plt.subplot(5, 2, 6)
plt.plot(corr_kinda_good, color = 'lightgreen')

plt.subplot(5,2,7)
plt.plot(kinda_bad, color = 'orange')
plt.subplot(5, 2, 8)
plt.plot(corr_kinda_bad, color = 'orange')

plt.subplot(5,2,9)
plt.plot(bad, color = 'red')
plt.subplot(5, 2, 10)
plt.plot(corr_bad, color = 'red')

plt.show()

print(reward_goal, reward_good, reward_kinda_good, reward_kinda_bad, reward_bad)