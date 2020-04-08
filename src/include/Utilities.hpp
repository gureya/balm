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

void read_weights(std::string filename);
void get_sum_nww_ww(int num_workers);

void hill_climbing_pmigration(void);
void hill_climbing_mba(void);
void hill_climbing_mba_10(void);
void hill_climbing_pmigration_100(void);
void hill_climbing_mba_sha(void);
void hill_climbing_pmigration_v2(void);

// Measurement functions
double get_target_stall_rate();
double get_percentile_latency();
double connect_to_client();

// Important Modes
void abc_numa(void);  // the overall solution i.e., page migration + mba
void page_migration_only(void);
void mba_only(void);
void linux_default(void);
void mba_10(void);
void disabled_controller(void);

// Important Functionalities
void apply_mba(int mba_value);
int search_optimal_mba();
int mba_binary_search(int current_mba, double progress);
int apply_pagemigration_rl(std::vector<MySharedMemory> mem_segments);
int apply_pagemigration_lr(std::vector<MySharedMemory> mem_segments);
int apply_pagemigration_rl_be(std::vector<MySharedMemory> mem_segments);
int check_opt_direction(std::vector<MySharedMemory> mem_segments);
int release_mba();
int apply_pagemigration_lr_dc(std::vector<MySharedMemory> mem_segments);

void signalHandler(int signum);

// test functions
void bw_manager_test(void);
void find_optimal_lr_ratio(void);
void my_logger(int crr, int cml, double hpt, double hcl, double hps, double bes,
               std::string);
void test_fixed_ratio(void);
void print_logs(void);

#endif /* INCLUDE_UTILITIES_HPP_ */
