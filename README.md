# ETH Advanced Operating System assignments for Barrelfish on ARMv7 Pandaboard. ##

### Building ###

```
$ mkdir build && cd build
$ <path/to/source/tree>/hake/hake.sh -s <path/to/source/tree> -a armv7
$ make -j7 PandaboardES
```

### Booting via USBBoot ###

```
$ sudo make usbboot_panda
```

### Reference ###
www.systems.ethz.ch/courses/fall2016/aos
