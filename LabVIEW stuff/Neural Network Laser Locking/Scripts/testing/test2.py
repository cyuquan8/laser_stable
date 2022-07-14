import numpy as np 
import matplotlib.pyplot as plt

# import waveforms
goal = np.loadtxt(".\\goal.csv")
good = np.loadtxt(".\\test_good.csv")
kinda_good = np.loadtxt(".\\test_kinda_good.csv")
kinda_bad = np.loadtxt(".\\test_kinda_bad.csv")
bad = np.loadtxt(".\\test_bad.csv")

# compute norms
norm_goal = np.linalg.norm(goal)
norm_good = np.linalg.norm(good)
norm_kinda_good = np.linalg.norm(kinda_good)
norm_kinda_bad = np.linalg.norm(kinda_bad)
norm_bad = np.linalg.norm(bad)

# compute correlation arrays
corr_auto = np.correlate(goal, goal, "full")/(norm_goal * norm_goal)
corr_good = np.correlate(goal, good, "full")[1:]/(norm_goal * norm_good)
corr_kinda_good = np.correlate(goal, kinda_good, "full") /(norm_goal * norm_kinda_good)
corr_bad = np.correlate(goal, bad, "full")/(norm_goal * norm_kinda_bad)
corr_kinda_bad = np.correlate(goal, kinda_bad, "full")/(norm_goal * norm_bad)

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