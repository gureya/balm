#!/bin/bash
#
# This script will report the time taken to execute a program
# given a CPU binding for x number of runs
#
# It will execute the program with all the (CPU) combinations
# and report its elapsed time
#
# Example usage:
# /path/to/check-scalability.sh /path/to/myApp appArg1 appArg2
#
# To configure the CPUs, change the arrays below!
log_file="/home/dgureya/devs/unstickymem/elapsed_stall_rate_log.txt"

#echo -e "3-WORKERS\n" >> ${log_file}

echo "streamcluster" >> ${log_file}
./run-app-xtimes_v3.sh streamcluster
echo "=================================================" >> ${log_file}

echo "/home/dgureya/devs/NPB3.0-omp-C/bin/sp.B" >> ${log_file}
./run-app-xtimes_v2.sh /home/dgureya/devs/NPB3.0-omp-C/bin/sp.B
echo "=================================================" >> ${log_file}

echo "/home/dgureya/devs/fft_openmp" >> ${log_file}
./run-app-xtimes_v2.sh /home/dgureya/devs/fft_openmp
echo "=================================================" >> ${log_file}

