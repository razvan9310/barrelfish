import matplotlib.pyplot as plt
import numpy as np

from math import log10

sizes = [64, 6400, 640000]

sdma_rotate_times = [[9056583, 9657132, 10272182, 3671854, 4271867, 5457278, 3380028, 10256436, 5457458, 2779993],
                     [7799936, 7800599, 7200640, 6300599, 3000747, 7200884, 9600764, 3000590, 5400458, 3000737],
                     [8824067, 10683803, 12783663, 16083949, 13083638, 8283578, 10683468, 7683570, 11283738, 6783683]]

reg_rotate_times = [[564, 533, 547, 564, 559, 545, 562, 560, 550, 563],
                    [35929, 36972, 36491, 36532, 36214, 36481, 36173, 36240, 36714, 36440],
                    [133322070,133330533,133349656,133338860,133345244,133350214,133330109,133326373,133335184,133343432]]

fig, ax = plt.subplots()

ax.set_title('Rotate Runtime')
ax.set_xlabel('Matrix size (32-bit ints, in logscale)')
ax.set_ylabel('Cycles / 1000 (in logscale)')

ax.plot(sizes, [int(np.mean(times)) for times in sdma_rotate_times], 'bs',
        label='SDMA', markersize=12)
ax.plot(sizes, [int(np.mean(times)) for times in sdma_rotate_times],
        linewidth=2.0)

ax.plot(sizes, [int(np.mean(times)) for times in reg_rotate_times], 'g^',
        label='Standard', markersize=12)
ax.plot(sizes, [int(np.mean(times)) for times in reg_rotate_times],
        linewidth=2.0)

legend = ax.legend(loc='lower right')

ax.set_xscale('log')
s_arr = [6.4]
s_arr.extend(sizes)
s_arr.append(6400000)
plt.xticks(s_arr, ['', '8 x 8', '80 x 80', '800 x 800', ''])

ax.set_yscale('log')
c_arr = [1, 10, 100, 1000, 10000, 100000, 1000000]
plt.yticks([c * 1000 for c in c_arr], ['1', '10', '100', '1,000', '10,000', '100,000', '1 mil.'])

plt.grid()
plt.savefig('rotate-times.png')

print ('=== SDMA rotate ===')
for i in range(3):
	print('size=' + str(sizes[i]) + ': mean=' + str(int(np.mean(sdma_rotate_times[i])))
		  + ' std=' + str(int(np.std(sdma_rotate_times[i]))))

print ('\n=== Regular rotate ===')
for i in range(3):
    print('size=' + str(sizes[i]) + ': mean=' + str(int(np.mean(reg_rotate_times[i])))
          + ' std=' + str(int(np.std(reg_rotate_times[i]))))