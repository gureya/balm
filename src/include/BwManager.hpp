/*
 * bw-manager.hpp
 *
 *  Created on: May 31, 2019
 *      Author: david
 */

#ifndef INCLUDE_BWMANAGER_HPP_
#define INCLUDE_BWMANAGER_HPP_

#include <vector>

#ifdef __cplusplus
// check whether a monitoring core has been passed
extern bool MONITORED_CORES;
extern "C" {
#endif

// the monitoring cores
extern std::vector<int> BWMAN_CORES;
extern int active_cpus;
// Worker Node
extern int WORKER_NODE;
// Maximum number of nodes in the system
#define MAX_NODES 2
// The adaptation step
// TODO: Make this a command line parameter!
#define ADAPTATION_STEP 10  // E.g. Move 10% of shared pages to the worker nodes

void start_bw_manager(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* INCLUDE_BWMANAGER_HPP_ */
