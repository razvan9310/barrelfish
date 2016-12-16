import matplotlib.pyplot as plt
import numpy as np

from math import log10

sizes = [1099, 10990, 109900, 1099000, 10990000]

sdma_memset_times = [[701153, 702237, 2159670, 2778775, 942588, 2759553, 4559976, 942471, 1574694, 942738],
                     [7514445, 6917601, 9317649, 11717850, 6917664, 6917657, 6317848, 8117262, 9317568, 7517676],
                     [11996847,12027480,12027584,13827839,15627557,12627315,15027525,12627519,13827441,15627582],
                     [19810176,21627753,21627725,26427339,22227831,21027516,20427852,21027393,21027465,20427588],
                     [27757779,28530240,29130060,27330270,29730168,27930417,27330440,30930285,29130281,27629867]]

reg_memset_times = [[530, 572, 536, 533, 549, 547, 544, 537, 546, 539],
                    [2334, 2225, 2308, 2355, 2318, 2277, 2270, 2313, 2296, 2252],
                    [20469, 20467, 20416, 20499, 20335, 20454, 20462, 20450, 20416, 20394],
                    [203531, 203036, 203081, 203042, 202794, 202862, 203127, 203171, 203219, 202888],
                    [6318294, 6320266, 6318975, 6319836, 6320296, 6319589, 6319605, 6319637, 6319705, 6319488]]

fig, ax = plt.subplots()

ax.set_title('Memset Runtime')
ax.set_xlabel('Data size (bytes, in logscale)')
ax.set_ylabel('Cycles / 1000 (in logscale)')

ax.plot(sizes, [int(np.mean(times)) for times in sdma_memset_times], 'bs',
        label='SDMA', markersize=12)
ax.plot(sizes, [int(np.mean(times)) for times in sdma_memset_times],
        linewidth=2.0)

ax.plot(sizes, [int(np.mean(times)) for times in reg_memset_times], 'g^',
        label='Standard', markersize=12)
ax.plot(sizes, [int(np.mean(times)) for times in reg_memset_times],
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
plt.savefig('memset-times.png')

print ('=== SDMA memset ===')
for i in range(5):
	print('size=' + str(sizes[i]) + ': mean=' + str(int(np.mean(sdma_memset_times[i])))
		  + ' std=' + str(int(np.std(sdma_memset_times[i]))))

print ('\n=== Regular memset ===')
for i in range(5):
    print('size=' + str(sizes[i]) + ': mean=' + str(int(np.mean(reg_memset_times[i])))
          + ' std=' + str(int(np.std(reg_memset_times[i]))))