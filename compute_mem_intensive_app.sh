#!/bin/bash
#trap "kill 0" EXIT

#use swaptions as the warm-up phase
#numactl --physcpubind=1-9 --membind=1 /usr/bin/time -f%e /home/dgureya/parsec-3.0/pkgs/apps/swaptions/inst/amd64-linux.gcc-pthreads/bin/swaptions -ns 128 -sm 1000000 -nt 9

#use my benchmark as the warm-up phase
UNSTICKYMEM_MODE=disabled BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-7 --membind=0 /home/dgureya/adaptive_bw_bench/adaptive_bw_bench -c 0-7 -s 1 -p 16 -t 1 -y 100 -z 0 

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-8 --membind=0 /home/dgureya/parsec-3.0/ext/splash2x/apps/ocean_ncp/inst/amd64-linux.gcc/bin/ocean_ncp -n4098 -p8 -e1e-15 -r10000 -t12100

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-8 --membind=0 /home/dgureya/Splash-3/codes/apps/ocean/contiguous_partitions/OCEAN -n4098 -p8 -e1e-15 -r10000 -t12100

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-7 /home/dgureya/Splash-3/codes/apps/ocean/contiguous_partitions/OCEAN -n8194 -p8 -e1e-07 -r10000 -t14400

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-8 --membind=0 /home/dgureya/Splash-3/codes/apps/ocean/non_contiguous_partitions/OCEAN -n4098 -p8 -e1e-15 -r10000 -t12100

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-7 --membind=0 /home/dgureya/NPB-OMP-C-mirror/SP/sp.C.x

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-7 --membind=0 /home/dgureya/NPB-OMP-C-mirror/MG/mg.C.x

BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-7 --membind=0 /home/dgureya/NPB-OMP-C-mirror/UA/ua.C.x

#Get PID of the controller and kill it
pid1=$(ps aux | grep '[B]wManager' | awk '{print $2}')
kill -2 $pid1
