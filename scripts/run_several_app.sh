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
# log_file="/home/dgureya/devs/unstickymem/elapsed_stall_rate_log.txt"

#echo -e "3-WORKERS\n" >> ${log_file}

red=`tput setaf 1`

echo -e "${red}streamcluster"
echo -e "${red}============================================"
./sc.sh

echo -e "${red}ocean_cp"
echo -e "${red}============================================"
./oc.sh

echo -e "${red}ocean_ncp"
echo -e "${red}============================================"
./on.sh

#echo -e "${red}/home/dgureya/devs/NPB3.0-omp-C/bin/sp.B"
echo -e "${red}/home/dgureya/NPB3.0-omp-C/bin/sp.B"
echo -e "${red}============================================"
#./generic.sh /home/dgureya/devs/NPB3.0-omp-C/bin/sp.B
./generic.sh /home/dgureya/NPB3.0-omp-C/bin/sp.B

#echo -e "${red}/home/dgureya/devs/NPB3.0-omp-C/bin/bt.B"
echo -e "${red}/home/dgureya/NPB3.0-omp-C/bin/sp.B"
echo -e "${red}============================================"
#./generic.sh /home/dgureya/devs/NPB3.0-omp-C/bin/bt.B
./generic.sh /home/dgureya/NPB3.0-omp-C/bin/bt.B

