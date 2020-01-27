#include <iostream>
#include <sstream>

#include <fstream>
#include <string>
#include <cmath>
#include <csignal>

#include <stdlib.h>

#include "include/BwManager.hpp"
#include "include/Logger.hpp"
#include "include/PerformanceCounters.hpp"
#include "include/MySharedMemory.hpp"
#include "include/PagePlacement.hpp"
#include "include/Utilities.hpp"

// number of workers
static bool OPT_NUM_WORKERS = false;
bool MONITORED_CORES = false;
static bool WEIGHTS = false;
bool BWMAN_MODE = false;
bool FIXED_RATIO = false;

int BWMAN_WORKERS = 1;
int bwman_mode_value = 0;  // 0 - adaptive-coscheduled, 1 - fixed-ratio, 2 - adaptive-standalone
double fixed_ratio_value = 0;

static bool is_initialized = false;

// sum of worker nodes weights
double sum_ww = 0;
// sum of non-worker nodes weights
double sum_nww = 0;

using namespace std;

vector<int> BWMAN_CORES;
vector<pair<double, int>> BWMAN_WEIGHTS;
int active_cpus;

const char* monitored_cores_s;

void read_config(void) {

  LINFOF("NUMA NODES: %d", MAX_NODES);

  OPT_NUM_WORKERS = getenv("BWMAN_WORKERS") != nullptr;
  if (OPT_NUM_WORKERS) {
    BWMAN_WORKERS = stoi(std::getenv("BWMAN_WORKERS"));
  }

  MONITORED_CORES = getenv("BWMAN_CORES") != nullptr;
  if (MONITORED_CORES) {
    monitored_cores_s = getenv("BWMAN_CORES");
    LINFOF("monitoring_core: %s", monitored_cores_s);

    string s(monitored_cores_s);

    stringstream ss(s);

    string tok;
    char delimiter = ',';

    while (getline(ss, tok, delimiter)) {
      BWMAN_CORES.push_back(stoi(tok));
    }

    cout << "BWMAN_CORES: \t";
    for (size_t i = 0; i < BWMAN_CORES.size(); i++) {
      cout << BWMAN_CORES.at(i) << "\t";
    }
    cout << endl;

    active_cpus = BWMAN_CORES.size();
    if (active_cpus < 2) {
      LINFO(
          "At least provide 2 monitoring cores (co-scheduled applications > 2)");
      exit(EXIT_FAILURE);
    }

  } else {
    LINFO("At least provide 1 monitored core! e.g. BWMAN_CORES=0,10");
    exit(EXIT_FAILURE);
  }

  //check if the monitored cores vector is empty
  if (BWMAN_CORES.empty()) {
    LINFO("BWMAN_CORES vector is empty!");
    exit(EXIT_FAILURE);
  }

  BWMAN_MODE = getenv("BWMAN_MODE") != nullptr;
  if (BWMAN_MODE) {
    bwman_mode_value = stoi(getenv("BWMAN_MODE"));
  }
  LINFOF("BWMAN_MODE: %d", bwman_mode_value);

  FIXED_RATIO = getenv("FIXED_RATIO") != nullptr;
  if (FIXED_RATIO) {
    fixed_ratio_value = stof(getenv("FIXED_RATIO"));
  }
  LINFOF("FIXED_RATIO: %.2f", fixed_ratio_value);

  WEIGHTS = getenv("BWMAN_WEIGHTS") != nullptr;
  if (WEIGHTS) {
    char* weights = getenv("BWMAN_WEIGHTS");
    read_weights(weights);
  } else {
    LDEBUG(
        "Sorry, Weights have not been provided! e.g. BWMAN_WEIGHTS=weights/weights_1.txt");
    exit(EXIT_FAILURE);
  }

}

void start_bw_manager() {
  //periodic_monitor();
  //measure_stall_rate();
  find_optimal_lr_ratio();
  //bw_manager_test();
}

int main(int argc, char **argv) {

  // register signal SIGINT and signal handler
  signal(SIGINT, signalHandler);

  // parse and display the configuration
  read_config();

  /* if (BWMAN_WORKERS == MAX_NODES) {
   LINFOF("No. of workers equals MAX_NODES (%d==%d)! Exiting", BWMAN_WORKERS,
   MAX_NODES);
   destroy_shared_memory();
   exit(EXIT_FAILURE);
   } */

  //set sum_ww & sum_nww & initialize the weights!
  //get_sum_nww_ww(BWMAN_WORKERS);
  // initialize likwid
  initialize_likwid();

  is_initialized = true;
  LDEBUG("Initialized");

  start_bw_manager();

  //Destroy the shared memory be4 exiting!
  destroy_shared_memory();

  // stop all the counters
  stop_all_counters();
  LINFO("Finalized");

  return 0;
}

