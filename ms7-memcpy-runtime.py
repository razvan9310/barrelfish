import matplotlib.pyplot as plt
import numpy as np

from math import log10

sizes = [1099, 10990, 109900, 1099000, 10990000]

sdma_memcpy_times = [[5742000, 3666501, 4855317, 4881332, 2481300, 3389622, 7866444, 2454543, 3988713, 3654369],
		 			 [3077556, 3389688, 4866356, 5189508, 2454945, 2790516, 1588683, 3666874, 3081912, 5481538],
		 			 [8379834, 5717415, 3317781, 6017550, 3917629, 5717402, 6317898, 9017892, 4517325, 6617469],
		             [16135683, 3918003, 4517850, 5717358, 5417361, 3018015, 12017767, 6017403, 9617657, 13217919],
		             [10634316, 13517856, 7217634, 6317340, 12617847, 8717580, 13818087, 10217858, 10817733, 9017748]]

reg_memcpy_times = [[1982, 1891, 1957, 1942, 954, 2015, 1933, 1897, 2012, 1937],
                    [15625, 15596, 15595, 15580, 15614, 15601, 15580, 15604, 15688, 15679],
                    [153229, 153318, 153243, 153342, 153423, 153311, 153361, 153163, 153385, 153175],
                    [4594653, 4596525, 4595865, 4596153, 4596498, 4595796, 4595751, 4595413, 4595892, 4596000],
                    [47788575, 47788935, 47786223, 47787135, 47788734, 47790861, 47790231, 47788548, 47790444, 47787438]]

fig, ax = plt.subplots()

ax.set_title('Memcopy Runtime')
ax.set_xlabel('Data size (bytes, in logscale)')
ax.set_ylabel('Cycles / 1000 (in logscale)')

ax.plot(sizes, [int(np.mean(times)) for times in sdma_memcpy_times], 'bs',
        label='SDMA', markersize=12)
ax.plot(sizes, [int(np.mean(times)) for times in sdma_memcpy_times],
        linewidth=2.0)

ax.plot(sizes, [int(np.mean(times)) for times in reg_memcpy_times], 'g^',
        label='Standard', markersize=12)
ax.plot(sizes, [int(np.mean(times)) for times in reg_memcpy_times],
        linewidth=2.0)

legend = ax.legend(loc='lower right')

ax.set_xscale('log')
s_arr = [110]
s_arr.extend(sizes)
plt.xticks(s_arr, ['', '1,099', '10,990', '109,900', '1,099,000', '10,990,000'])

ax.set_yscale('log')
c_arr = [1, 10, 100, 1000, 10000, 100000]
plt.yticks([c * 1000 for c in c_arr], ['1', '10', '100', '1,000', '10,000', '100,000'])

plt.grid()
plt.savefig('memcpy-time.png')

print ('=== SDMA memcpy ===')
for i in range(5):
	print('size=' + str(sizes[i]) + ': mean=' + str(int(np.mean(sdma_memcpy_times[i])))
		  + ' std=' + str(int(np.std(sdma_memcpy_times[i]))))

print ('\n=== Regular memcpy ===')
for i in range(5):
    print('size=' + str(sizes[i]) + ': mean=' + str(int(np.mean(reg_memcpy_times[i])))
          + ' std=' + str(int(np.std(reg_memcpy_times[i]))))