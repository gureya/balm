#include "include/MbaHandler.hpp"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include/Logger.hpp"
#include "pqos.h"

/**
 * MBA struct type
 */
enum mba_type { REQUESTED = 0, ACTUAL, MAX_MBA_TYPES };

/**
 * Maintains number of MBA COS to be set
 */
static int sel_mba_cos_num = 0;

/**
 * Table containing  MBA requested and actual COS definitions
 * Requested is set by the user
 * Actual is set by the library
 */
static struct pqos_mba mba[MAX_MBA_TYPES];

struct pqos_config cfg;
const struct pqos_cpuinfo *p_cpu = NULL;
const struct pqos_cap *p_cap = NULL;
unsigned mba_id_count, *p_mba_ids = NULL;
int ret;

void initialize_mba() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.fd_log = STDOUT_FILENO;
  cfg.verbose = 0;
  /* PQoS Initialization - Check and initialize MBA capability */
  ret = pqos_init(&cfg);
  if (ret != PQOS_RETVAL_OK) {
    LINFO("Error initializing PQoS library!");
    exit(EXIT_FAILURE);
  }
  /* Get capability and CPU info pointers */
  ret = pqos_cap_get(&p_cap, &p_cpu);
  if (ret != PQOS_RETVAL_OK) {
    LINFO("Error retrieving PQoS capabilities!");
    exit(EXIT_FAILURE);
  }
  /* Get CPU mba_id information to set COS */
  p_mba_ids = pqos_cpu_get_mba_ids(p_cpu, &mba_id_count);
  if (p_mba_ids == NULL) {
    LINFO("Error retrieving MBA ID information!");
    exit(EXIT_FAILURE);
  }

  LINFO("Success initializing PQoS library!");
}

int set_mba_allocation(const unsigned socket_id) {
  ret = pqos_mba_set(socket_id, sel_mba_cos_num, &mba[REQUESTED], &mba[ACTUAL]);
  if (ret != PQOS_RETVAL_OK) {
    LINFO("Failed to set MBA!");
    return -1;
  }
  LINFOF("SKT%u: MBA COS%u => %u%% requested, %u%% applied", socket_id,
         mba[REQUESTED].class_id, mba[REQUESTED].mb_max, mba[ACTUAL].mb_max);

  return sel_mba_cos_num;
}

void set_mba_parameters(const unsigned cos_value, const uint64_t mba_value) {
  mba[REQUESTED].class_id = cos_value;
  mba[REQUESTED].mb_max = mba_value;
  mba[REQUESTED].ctrl = 0;
  sel_mba_cos_num = 1;
}

void reset_mba() {
  /*set mba back to 100 i.e. default before quitting*/
  set_mba_parameters(1, 100);
  ret = set_mba_allocation(0);
  /* reset and deallocate all the resources */
  ret = pqos_fini();
  if (ret != PQOS_RETVAL_OK) {
    LINFO("Error shutting down PQoS library!");
  } else {
    LINFO("Success shutting down PQoS library");
  }
  if (p_mba_ids != NULL) free(p_mba_ids);
}
