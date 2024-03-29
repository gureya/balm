/*
 * bw-manager.hpp
 *
 *  Created on: May 31, 2019
 *      Author: david
 */

#ifndef INCLUDE_BWMANAGER_HPP_
#define INCLUDE_BWMANAGER_HPP_

#include <inttypes.h>
#include <numa.h>
#include <sys/time.h>

#include <string>
#include <vector>

#ifdef __cplusplus
// check whether a monitoring core has been passed
extern bool MONITORED_CORES;
extern "C" {
#endif

// the monitoring cores
extern std::vector<int> BWMAN_CORES;
extern int active_cpus;
extern int fixed_ratio_value;

extern double target_slo;
extern double target_slo_xapian;
extern std::string server;
extern int port;
extern int current_remote_ratio;
extern int optimal_mba;
extern double delta_hp;  // operational region of the controller (5%) - HP
extern double delta_be;  // operational region of the controller (5%) - BE

// Worker Node
extern int BWMAN_WORKERS;
// Maximum number of nodes in the system
#define MAX_NODES 2
//#define MAX_NODES numa_num_configured_nodes()
// the vector to hold the weghts,id pair
extern std::vector<std::pair<double, int>> BWMAN_WEIGHTS;
// sum of worker nodes weights
extern double sum_ww;
// sum of non-worker nodes weights
extern double sum_nww;
// The adaptation step
// TODO: Make this a command line parameter!
#define ADAPTATION_STEP 10  // E.g. Move 10% of shared pages to the worker nodes

void start_bw_manager(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* INCLUDE_BWMANAGER_HPP_ */
