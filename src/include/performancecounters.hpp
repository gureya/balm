/*
 * performancecounters.hpp
 *
 *  Created on: May 31, 2019
 *      Author: david
 */

#ifndef INCLUDE_PERFORMANCECOUNTERS_HPP_
#define INCLUDE_PERFORMANCECOUNTERS_HPP_

#include <cstdint>

#include <string>

void initialize_likwid();


double get_stall_rate_v2();  // via Like I Knew What I'm Doing (LIKWID Library!)
void stop_all_counters();  // Restarting it might have some issues if counters are not stopped!
double get_elapsed_stall_rate();  //get the elapsed stall rate

// samples stall rate multiple times and filters outliers
double get_average_stall_rate(size_t num_measurements,
                              useconds_t usec_between_measurements,
                              size_t num_outliers_to_filter);

// read time stamp counter
inline uint64_t readtsc(void);

// read performance monitor counter
inline uint64_t readpmc(int32_t n);


#endif /* INCLUDE_PERFORMANCECOUNTERS_HPP_ */
