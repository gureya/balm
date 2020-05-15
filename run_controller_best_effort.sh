#!/bin/bash
trap "kill 0" EXIT

BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=1-9 --membind=0 /home/dgureya/adaptive_bw_bench/adaptive_bw_bench -c 1-9 -s 1 -p 16 -t 1 -y 300 -z 2800 &

numactl --physcpubind=0 /home/dgureya/numa-bw-manager/BwManager -s 146.193.41.52 -r 30 -m 3 

#BWAP_WEIGHTS=/home/dgureya/bwap/weights/weights_1w.txt /usr/bin/time -f%e numactl --physcpubind=0-8 --membind=0 /home/dgureya/adaptive_bw_bench/adaptive_bw_bench -c 0-8 -s 1 -p 16 -t 1 -y 300 -z 2800


sleep 60
#Get PIDs of the Apps
#pid1=$(ps aux | grep '[B]wManager' | awk '{print $2}')
#kill -9 $pid1
#pid2=$(ps aux | grep '[b]ench' | awk '{print $2}')
#kill -9 $pid2
