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

#log file where the results are collected
log_file="oc_results.txt"

#For Hydra (1,2,3,4,8 workers!)
cpus=("0-7" "0-15" "0-31" "0-63")
num_threads=("8" "16" "32" "64")
num_workers=("1" "2" "4" "8")
interleave_nodes=("0" "0-1" "0-3" "all")
weights=("weights_1w.txt" "weights_2w.txt" "weights_4w.txt" "weights_8w.txt")
weights_uniform=("uniform_weights.txt")
weights_workers=("w_weights_1w.txt" "w_weights_2w.txt" "w_weights_4w.txt" "w_weights_8w.txt")

#cpus=("0-7")
#num_threads=("8")
#num_workers=("1")
#interleave_nodes=("0")

#For ocean_cp's
#cpus=("0-7" "0-15" "0-31" "0-63")
#num_threads=("8" "16" "32" "64")
#num_workers=("1" "2" "4" "8")
#interleave_nodes=("0" "0-1" "0-3" "all")

run_cmd () {
  echo ${red}$@${reset}
  $@ > /dev/null
}

echo "Executable and args: ${@}"
echo "CPUs: ${cpus[@]}"
echo "NUM_THREADs: ${num_threads[@]}"
echo "WEIGHTS: ${weights[@]}"
echo "WEIGHTS_UNIFORM: ${weights_uniform[@]}"

red=`tput setaf 1`
reset=`tput sgr0`

echo "----------------------" >> ${log_file}
echo "|ocean_cp|" >> ${log_file}
echo -e "----------------------\n" >> ${log_file}

declare total=0

echo "bwap-canonical" >> ${log_file}
echo -e "${red}bwap-canonical"
echo -e "${red}============================================"

echo "==========" >> ${log_file}

for (( i=0; i<${#cpus[@]}; i++ ));
do
	echo -e "#threads:\t${num_threads[$i]}" >> ${log_file}
	echo -e "---------------------" >> ${log_file}
	total=0
	for x in {1..5}
	do
	
		echo -e "${red}Cores: ${cpus[$i]}"
		echo -e "${red}Num_threads: ${num_threads[$i]}"
		echo -e "${red}Num_workers: ${num_workers[$i]}"
		echo -e "${red}Weights: ${weights[$i]}"
		export OMP_NUM_THREADS=${num_threads[$i]}
	    export UNSTICKYMEM_MODE=scan
	    export UNSTICKYMEM_WORKERS=${num_workers[$i]}
		export BWAP_WEIGHTS=/home/dgureya/devs/unstickymem/config/${weights[$i]}
		START=$(date +%s)
	    	#run_cmd "numactl --physcpubind=${cpus[$i]} $@"
		#if [ "${num_workers[$i]}" != "8" ]; then
		#run_cmd "numactl --physcpubind=${cpus[$i]} $@"
		#run_cmd "/home/dgureya/devs/bandwidth_bench_shared -c ${cpus[$i]}"
	    run_cmd "numactl --physcpubind=${cpus[$i]} /home/dgureya/parsec-3.0/ext/splash2x/apps/ocean_cp/inst/amd64-linux.gcc/bin/ocean_cp -n4098 -p${num_threads[$i]} -e1e-14 -r10000 -t14400"
		END=$(date +%s)
		DIFF=$(( $END - $START ))
		echo -e "Run_$x\t$DIFF" >> ${log_file}
		total=$(($total + $DIFF))
		unset UNSTICKYMEM_MODE
		unset UNSTICKYMEM_WORKERS
		unset BWAP_WEIGHTS
    done
    AVG=$(($total / 5))
    echo -e "Average:\t$AVG\n" >> ${log_file}
done


echo "uniform-all" >> ${log_file}
echo "==========" >> ${log_file}

echo -e "${red}uniform-all"
echo -e "${red}============================================"

for (( i=0; i<${#cpus[@]}; i++ ));
do
	echo -e "#threads:\t${num_threads[$i]}" >> ${log_file}
	echo -e "---------------------" >> ${log_file}
	total=0
	for x in {1..5}
	do
	
		echo -e "${red}Cores: ${cpus[$i]}"
		echo -e "${red}Num_threads: ${num_threads[$i]}"
		echo -e "${red}Num_workers: ${num_workers[$i]}"
		echo -e "${red}Weights: ${weights_uniform[0]}"
		export OMP_NUM_THREADS=${num_threads[$i]}
	    export UNSTICKYMEM_MODE=scan
	    export UNSTICKYMEM_WORKERS=${num_workers[$i]}
		export BWAP_WEIGHTS=/home/dgureya/devs/unstickymem/config/${weights_uniform[0]}
		START=$(date +%s)
	    	#run_cmd "numactl --physcpubind=${cpus[$i]} $@"
		#if [ "${num_workers[$i]}" != "8" ]; then
		#run_cmd "numactl --physcpubind=${cpus[$i]} $@"
		#run_cmd "/home/dgureya/devs/bandwidth_bench_shared -c ${cpus[$i]}"
	    run_cmd "numactl --physcpubind=${cpus[$i]} /home/dgureya/parsec-3.0/ext/splash2x/apps/ocean_cp/inst/amd64-linux.gcc/bin/ocean_cp -n4098 -p${num_threads[$i]} -e1e-14 -r10000 -t14400"
		END=$(date +%s)
		DIFF=$(( $END - $START ))
		echo -e "Run_$x\t$DIFF" >> ${log_file}
		total=$(($total + $DIFF))
		unset UNSTICKYMEM_MODE
		unset UNSTICKYMEM_WORKERS
		unset BWAP_WEIGHTS
    done
    AVG=$(($total / 5))
    echo -e "Average:\t$AVG\n" >> ${log_file}
done

echo "uniform-workers" >> ${log_file}
echo "==========" >> ${log_file}

echo -e "${red}uniform-workers"
echo -e "${red}============================================"

for (( i=0; i<${#cpus[@]}; i++ ));
do
	echo -e "#threads:\t${num_threads[$i]}" >> ${log_file}
	echo -e "---------------------" >> ${log_file}
	total=0
	for x in {1..5}
	do
	
		echo -e "${red}Cores: ${cpus[$i]}"
		echo -e "${red}Num_threads: ${num_threads[$i]}"
		echo -e "${red}Num_workers: ${num_workers[$i]}"
		echo -e "${red}Weights: ${weights_workers[$i]}"
		export OMP_NUM_THREADS=${num_threads[$i]}
	    export UNSTICKYMEM_MODE=scan
	    export UNSTICKYMEM_WORKERS=${num_workers[$i]}
		export BWAP_WEIGHTS=/home/dgureya/devs/unstickymem/config/${weights_workers[$i]}
		START=$(date +%s)
	    	#run_cmd "numactl --physcpubind=${cpus[$i]} $@"
		#if [ "${num_workers[$i]}" != "8" ]; then
		#run_cmd "numactl --physcpubind=${cpus[$i]} $@"
		#run_cmd "/home/dgureya/devs/bandwidth_bench_shared -c ${cpus[$i]}"
	    run_cmd "numactl --physcpubind=${cpus[$i]} /home/dgureya/parsec-3.0/ext/splash2x/apps/ocean_cp/inst/amd64-linux.gcc/bin/ocean_cp -n4098 -p${num_threads[$i]} -e1e-14 -r10000 -t14400"
		END=$(date +%s)
		DIFF=$(( $END - $START ))
		echo -e "Run_$x\t$DIFF" >> ${log_file}
		total=$(($total + $DIFF))
		unset UNSTICKYMEM_MODE
		unset UNSTICKYMEM_WORKERS
		unset BWAP_WEIGHTS
    done
    AVG=$(($total / 5))
    echo -e "Average:\t$AVG\n" >> ${log_file}
done

echo "default" >> ${log_file}
echo "==========" >> ${log_file}

echo -e "${red}default"
echo -e "${red}============================================"

for (( i=0; i<${#cpus[@]}; i++ ));
do
	echo -e "#threads:\t${num_threads[$i]}" >> ${log_file}
	echo -e "---------------------" >> ${log_file}
	total=0
	for x in {1..5}
	do
	
		echo -e "${red}Cores: ${cpus[$i]}"
		echo -e "${red}Num_threads: ${num_threads[$i]}"
		echo -e "${red}Num_workers: ${num_workers[$i]}"
		echo -e "${red}Weights: ${weights[$i]}"
		export OMP_NUM_THREADS=${num_threads[$i]}
	    export UNSTICKYMEM_MODE=disabled
	    export UNSTICKYMEM_WORKERS=${num_workers[$i]}
		export BWAP_WEIGHTS=/home/dgureya/devs/unstickymem/config/${weights_workers[$i]}
		START=$(date +%s)
	    	#run_cmd "numactl --physcpubind=${cpus[$i]} $@"
		#if [ "${num_workers[$i]}" != "8" ]; then
		#run_cmd "numactl --physcpubind=${cpus[$i]} $@"
		#run_cmd "/home/dgureya/devs/bandwidth_bench_shared -c ${cpus[$i]}"
	    run_cmd "numactl --physcpubind=${cpus[$i]} /home/dgureya/parsec-3.0/ext/splash2x/apps/ocean_cp/inst/amd64-linux.gcc/bin/ocean_cp -n4098 -p${num_threads[$i]} -e1e-14 -r10000 -t14400"
		END=$(date +%s)
		DIFF=$(( $END - $START ))
		echo -e "Run_$x\t$DIFF" >> ${log_file}
		total=$(($total + $DIFF))
		unset UNSTICKYMEM_MODE
		unset UNSTICKYMEM_WORKERS
		unset BWAP_WEIGHTS
    done
    AVG=$(($total / 5))
    echo -e "Average:\t$AVG\n" >> ${log_file}
done

