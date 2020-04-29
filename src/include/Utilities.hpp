/*
 * Utilities.hpp
 *
 *  Created on: Jan 7, 2020
 *      Author: David Daharewa Gureya
 */

#ifndef INCLUDE_UTILITIES_HPP_
#define INCLUDE_UTILITIES_HPP_

#include <vector>

// for printing the timestamps
#include <chrono>
#include <ctime>

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
int search_optimal_mba(void);
int mba_binary_search(int current_mba, double progress);
int apply_pagemigration_rl(void);
int apply_pagemigration_lr(void);
int apply_pagemigration_rl_be(void);
int check_opt_direction(void);
int release_mba(void);
int apply_pagemigration_lr_dc(void);
void get_memory_segments(void);

void signalHandler(int signum);
void terminateHandler(void);

// test functions
void bw_manager_test(void);
void find_optimal_lr_ratio(void);
void my_logger(std::chrono::system_clock::time_point tn, int crr, int cml,
               double hpt, double hcl, double slk, double hps, double bes,
               std::string, int lc);
void test_fixed_ratio(void);
void print_logs(void);
void print_logs_v2(void);
void print_to_file(void);

#endif /* INCLUDE_UTILITIES_HPP_ */
