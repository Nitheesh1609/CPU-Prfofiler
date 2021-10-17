#!/bin/bash
sudo insmod project3.ko
cat /proc/perftop
sudo rmmod project3.ko
