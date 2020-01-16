/*
 * Utilities.hpp
 *
 *  Created on: Jan 7, 2020
 *      Author: David Daharewa Gureya
 */

#ifndef INCLUDE_UTILITIES_HPP_
#define INCLUDE_UTILITIES_HPP_

void read_weights(char filename[]);
void get_sum_nww_ww(int num_workers);

void hill_climbing_pmigration(void);
void hill_climbing_mba(void);
void hill_climbing_mba_10(void);
void hill_climbing_pmigration_100(void);
void hill_climbing_mba_sha(void);
void hill_climbing_pmigration_v2(void);

double get_target_stall_rate(void);
void periodic_monitor(void);
int apply_mba(double target_stall_rate);
int binary_search(int current_mba, double progress);
int apply_pagemigration_rl(double target_stall_rate, int current_remote_ratio);
int apply_pagemigration_lr(double target_stall_rate, int current_remote_ratio);

void signalHandler(int signum);

#endif /* INCLUDE_UTILITIES_HPP_ */
