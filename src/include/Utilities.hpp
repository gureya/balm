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




#endif /* INCLUDE_UTILITIES_HPP_ */
