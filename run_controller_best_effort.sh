#!/bin/bash
trap "kill 0" EXIT

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-9 --membind=0 /home/dgureya/adaptive_bw_bench/adaptive_bw_bench -c 1-9 -s 1 -p 16 -t 1 -y 300 -z 2800 &

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-8 --membind=0 /home/dgureya/parsec-3.0/ext/splash2x/apps/ocean_ncp/inst/amd64-linux.gcc/bin/ocean_ncp -n4098 -p8 -e1e-15 -r10000 -t12100 &

#UNSTICKYMEM_MODE=disabled BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-9 --membind=1 /home/dgureya/adaptive_bw_bench/adaptive_bw_bench -c 1-9 -s 1 -p 16 -t 1 -y 100 -z 0

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-8 --membind=0 /home/dgureya/Splash-3/codes/apps/ocean/contiguous_partitions/OCEAN -n16386 -p8 &

#use swaptions as the warm-up phase
#numactl --physcpubind=1-9 --membind=0 /home/dgureya/parsec-3.0/pkgs/apps/swaptions/inst/amd64-linux.gcc-pthreads/bin/swaptions -ns 128 -sm 1000000 -nt 9

#UNSTICKYMEM_MODE=disabled BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-9 --membind=1 /home/dgureya/adaptive_bw_bench/adaptive_bw_bench -c 1-9 -s 1 -p 16 -t 1 -y 100 -z 0

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-8 /home/dgureya/Splash-3/codes/apps/ocean/contiguous_partitions/OCEAN -n4098 -p8 -e1e-15 -r10000 -t12100 &

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-8 /home/dgureya/Splash-3/codes/apps/ocean/contiguous_partitions/OCEAN -n8194 -p8 -e1e-07 -r10000 -t14400 &

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-7 --membind=0 /home/dgureya/Splash-3/codes/apps/ocean/non_contiguous_partitions/OCEAN -n4098 -p8 -e1e-15 -r10000 -t12100 &

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-9 --membind=0 /home/dgureya/NPB3.0-omp-C/bin/mg.C &

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-9 --membind=0 /home/dgureya/NPB-OMP-C-mirror/MG/mg.C.x &

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-7 --membind=0 /home/dgureya/NPB-OMP-C-mirror/SP/sp.C.x &

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-9 --membind=0 /home/dgureya/NPB-OMP-C-mirror/UA/ua.C.x &

/home/dgureya/numa-bw-manager/compute_mem_intensive_app.sh &

#sleep 7

numactl --physcpubind=16-31,48-63 --membind=1 /home/dgureya/numa-bw-manager/BwManager -s 146.193.41.56 -r 40 -m 3 -t 600
#/home/dgureya/numa-bw-manager/BwManager -s 146.193.41.56 -r 0 -m 5 -t 600

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-8 --membind=0 /home/dgureya/adaptive_bw_bench/adaptive_bw_bench -c 0-8 -s 1 -p 16 -t 1 -y 300 -z 2800


#sleep 60
#Get PIDs of the Apps
#pid1=$(ps aux | grep '[B]wManager' | awk '{print $2}')
#kill -9 $pid1
#pid2=$(ps aux | grep '[b]ench' | awk '{print $2}')
#kill -9 $pid2
