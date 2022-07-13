import numpy as np 
import matplotlib.pyplot as plt
from utils.utils import dynamic_peak_finder

waveform_threshold_ratio = 0.9
waveform_prominence = (0.2, None)
number_of_peaks_threshold = 3
reward_multiplier_constant = 1000

# import waveforms
s1_goal = np.loadtxt(".\\goal.csv")
s1_good = np.loadtxt(".\\test_good.csv")
s1_kinda_good = np.loadtxt(".\\test_kinda_good.csv")
s1_kinda_bad = np.loadtxt(".\\test_kinda_bad.csv")
s1_bad = np.loadtxt(".\\test_bad.csv")

# compute norms
norm_goal = np.linalg.norm(s1_goal)
norm_good = np.linalg.norm(s1_good)
norm_kinda_good = np.linalg.norm(s1_kinda_good)
norm_kinda_bad = np.linalg.norm(s1_kinda_bad)
norm_bad = np.linalg.norm(s1_bad)

# norm_goal = 1
# norm_good = 1
# norm_kinda_good = 1
# norm_kinda_bad = 1
# norm_bad = 1

# subtract mean
s2_goal = s1_goal - s1_goal.mean()
s2_good = s1_good - s1_good.mean()
s2_kinda_good = s1_kinda_good - s1_kinda_good.mean()
s2_kinda_bad = s1_kinda_bad - s1_kinda_bad.mean()
s2_bad = s1_bad - s1_bad.mean()

# scale according to max
s3_goal = s2_goal / max(abs(s2_goal))
s3_good = s2_good / max(abs(s2_good))
s3_kinda_good = s2_kinda_good / max(abs(s2_kinda_good))
s3_kinda_bad = s2_kinda_bad / max(abs(s2_kinda_bad))
s3_bad = s2_bad / max(abs(s2_bad))

# # thresholding
# s3_goal[abs(s3_goal) <= waveform_threshold_ratio * abs(max(s3_goal))] = 0
# s3_good[abs(s3_good) <= waveform_threshold_ratio * abs(max(s3_good))] = 0
# s3_kinda_good[abs(s3_kinda_good) <= waveform_threshold_ratio * abs(max(s3_kinda_good))] = 0
# s3_kinda_bad[abs(s3_kinda_bad) <= waveform_threshold_ratio * abs(max(s3_kinda_bad))] = 0
# s3_bad[abs(s3_bad) <= waveform_threshold_ratio * abs(max(s3_bad))] = 0

# dpf
s4_goal = dynamic_peak_finder(s3_goal, waveform_prominence, number_of_peaks_threshold)
s4_good = dynamic_peak_finder(s3_good, waveform_prominence, number_of_peaks_threshold)
s4_kinda_good = dynamic_peak_finder(s3_kinda_good, waveform_prominence, number_of_peaks_threshold)
s4_kinda_bad = dynamic_peak_finder(s3_kinda_bad, waveform_prominence, number_of_peaks_threshold)
s4_bad = dynamic_peak_finder(s3_bad, waveform_prominence, number_of_peaks_threshold)

# # number of peaks threshold
# goal = number_of_peaks_thresholding(goal, number_of_peaks_threshold)
# good = number_of_peaks_thresholding(good, number_of_peaks_threshold)
# kinda_good = number_of_peaks_thresholding(kinda_good, number_of_peaks_threshold)
# kinda_bad = number_of_peaks_thresholding(kinda_bad, number_of_peaks_threshold)
# bad = number_of_peaks_thresholding(bad, number_of_peaks_threshold)

# compute correlation arrays
corr_goal = np.correlate(s4_goal, s4_goal, "full")/(norm_goal * norm_goal) * reward_multiplier_constant
corr_good = np.correlate(s4_goal, s4_good, "full")/(norm_goal * norm_good) * reward_multiplier_constant
corr_kinda_good = np.correlate(s4_goal, s4_kinda_good, "full") /(norm_goal * norm_kinda_good) * reward_multiplier_constant
corr_bad = np.correlate(s4_goal, s4_kinda_bad, "full")/(norm_goal * norm_kinda_bad) * reward_multiplier_constant
corr_kinda_bad = np.correlate(s4_goal, s4_bad, "full")/(norm_goal * norm_bad) * reward_multiplier_constant

#
reward_goal = np.sum(np.absolute(corr_goal))
reward_good = np.sum(np.absolute(corr_good))
reward_kinda_good = np.sum(np.absolute(corr_kinda_good))
reward_kinda_bad = np.sum(np.absolute(corr_kinda_bad))
reward_bad = np.sum(np.absolute(corr_bad))

# plot
plt.subplot(5, 5, 1)
plt.plot(s1_goal)
plt.subplot(5, 5, 2)
plt.plot(s2_goal)
plt.subplot(5, 5, 3)
plt.plot(s3_goal)
plt.subplot(5, 5, 4)
plt.plot(s4_goal)
plt.subplot(5, 5, 5)
plt.plot(corr_goal)

plt.subplot(5, 5, 6)
plt.plot(s1_good, color = 'green')
plt.subplot(5, 5, 7)
plt.plot(s2_good, color = 'green')
plt.subplot(5, 5, 8)
plt.plot(s3_good, color = 'green')
plt.subplot(5, 5, 9)
plt.plot(s4_good, color = 'green')
plt.subplot(5, 5, 10)
plt.plot(corr_good, color = 'green')

plt.subplot(5, 5, 11)
plt.plot(s1_kinda_good, color = 'lightgreen')
plt.subplot(5, 5, 12)
plt.plot(s2_kinda_good, color = 'lightgreen')
plt.subplot(5, 5, 13)
plt.plot(s3_kinda_good, color = 'lightgreen')
plt.subplot(5, 5, 14)
plt.plot(s4_kinda_good, color = 'lightgreen')
plt.subplot(5, 5, 15)
plt.plot(corr_kinda_good, color = 'lightgreen')

plt.subplot(5, 5, 16)
plt.plot(s1_kinda_bad, color = 'orange')
plt.subplot(5, 5, 17)
plt.plot(s2_kinda_bad, color = 'orange')
plt.subplot(5, 5, 18)
plt.plot(s3_kinda_bad, color = 'orange')
plt.subplot(5, 5, 19)
plt.plot(s4_kinda_bad, color = 'orange')
plt.subplot(5, 5, 20)
plt.plot(corr_kinda_bad, color = 'orange')

plt.subplot(5, 5, 21)
plt.plot(s1_bad, color = 'red')
plt.subplot(5, 5, 22)
plt.plot(s2_bad, color = 'red')
plt.subplot(5, 5, 23)
plt.plot(s3_bad, color = 'red')
plt.subplot(5, 5, 24)
plt.plot(s4_bad, color = 'red')
plt.subplot(5, 5, 25)
plt.plot(corr_bad, color = 'red')

print(reward_goal, reward_good, reward_kinda_good, reward_kinda_bad, reward_bad)

plt.show()