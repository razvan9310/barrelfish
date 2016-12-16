import matplotlib.pyplot as plt

x = [4096, 40960, 409600, 4096000]
dma = [212138722, ]

x = [3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 11000, 12000,
     13000, 14000, 15000, 16000]
np8 = [3.47624204, 2.7029645, 2.30183305, 2.17490748,
       2.07369358, 2.06024378, 1.99619183, 2.01925624, 2.0103789, 1.93391597,
       1.90023483, 1.98055693, 1.95834929, 1.9753467]
np16 = [3.34955131, 2.6740761, 2.20899953, 2.07154934,
        1.95355746, 1.86085342, 1.84787637, 1.72226761, 1.80951188, 1.83056273,
        1.70828838, 1.64570496, 1.67373119, 1.71566537]

fig, ax = plt.subplots()

ax.set_title('Delta-Stepping: NY road network (264346 vertices)')
ax.set_xlabel('Delta / 1000')
ax.set_ylabel('Runtime (s)')

ax.plot(x, np8, 'bs', label='8 cores', markersize=12)
ax.plot(x, np8, linewidth=2.0)

ax.plot(x, np16, 'g^', label='16 cores', markersize=12)
ax.plot(x, np16, linewidth=2.0)

legend = ax.legend(loc='upper right')

plt.xticks(x, [i / 1000 for i in x])
plt.yticks([0, 0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3,
	        3.25, 3.5, 3.75])

plt.grid()
plt.savefig('varying-delta-ny.png')