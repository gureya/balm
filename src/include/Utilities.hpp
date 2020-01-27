/*
 * Utilities.hpp
 *
 *  Created on: Jan 7, 2020
 *      Author: David Daharewa Gureya
 */

#ifndef INCLUDE_UTILITIES_HPP_
#define INCLUDE_UTILITIES_HPP_

#include <vector>
#include "include/MySharedMemory.hpp"

void read_weights(char filename[]);
void get_sum_nww_ww(int num_workers);

void hill_climbing_pmigration(void);
void hill_climbing_mba(void);
void hill_climbing_mba_10(void);
void hill_climbing_pmigration_100(void);
void hill_climbing_mba_sha(void);
void hill_climbing_pmigration_v2(void);

double get_target_stall_rate(int current_remote_ratio);
void periodic_monitor(void);
void apply_mba(int mba_value);
int search_optimal_mba(double target_stall_rate, int current_optimal_mba);
int mba_binary_search(int current_mba, double progress);
int apply_pagemigration_rl(double target_stall_rate, int current_remote_ratio,
                           std::vector<MySharedMemory> mem_segments);
int apply_pagemigration_lr(double target_stall_rate, int current_remote_ratio,
                           std::vector<MySharedMemory> mem_segments);
int release_mba(int optimal_mba, double target_stall_rate,
                int current_remote_ratio);

void signalHandler(int signum);

//test functions
void bw_manager_test(void);
void measure_stall_rate(void);
void find_optimal_lr_ratio(void);
void my_logger(int crr, int cml, double hpt, double hps, double bes);
void test_fixed_ratio();

#endif /* INCLUDE_UTILITIES_HPP_ */
