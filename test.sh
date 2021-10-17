!#/bin/bash
make clean
make
sudo insmod project3.ko
cat /proc/perftop
cat /proc/perftop
sudo rmmod project3.ko

