/*
 * performancecounters.hpp
 *
 *  Created on: May 31, 2019
 *      Author: david
 */

#ifndef INCLUDE_PERFORMANCECOUNTERS_HPP_
#define INCLUDE_PERFORMANCECOUNTERS_HPP_

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>

#include <likwid.h>

#include <numa.h>
#include <numaif.h>

//format specifiers for the intN_t types
#include <inttypes.h>

#include "include/BwManager.hpp"
#include "include/Logger.hpp"

void initialize_likwid();

std::vector<double> get_stall_rate();  // via Like I Knew What I'm Doing (LIKWID Library!)
void stop_all_counters();  // Restarting it might have some issues if counters are not stopped!

//start and stop counters when placing pages
void start_counters();
void stop_counters();

// samples stall rate multiple times and filters outliers
std::vector<double> get_average_stall_rate(int num_measurements,
                                           useconds_t usec_between_measurements,
                                           int num_outliers_to_filter);

// read time stamp counter
inline uint64_t readtsc(void);

// read performance monitor counter
inline uint64_t readpmc(int32_t n);

#endif /* INCLUDE_PERFORMANCECOUNTERS_HPP_ */
