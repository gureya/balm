/*
 * bw-manager.hpp
 *
 *  Created on: May 31, 2019
 *      Author: david
 */

#ifndef INCLUDE_BW_MANAGER_HPP_
#define INCLUDE_BW_MANAGER_HPP_

#include <vector>

#ifdef __cplusplus
// check whether a monitoring core has been passed
extern bool MONITORED_CORES;
extern "C" {
#endif

// the monitoring cores
extern std::vector<int> MONITORED_CORES_VALUE;
// Maximum number of nodes in the system

// The adaptation step
// TODO: Make this a command line parameter!
#define ADAPTATION_STEP 10  // E.g. Move 10% of shared pages to the worker nodes

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* INCLUDE_BW_MANAGER_HPP_ */
