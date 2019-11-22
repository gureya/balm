#include <iostream>
#include <sstream>

#include <fstream>
#include <string>
#include <cmath>

#include <stdlib.h>

#include "include/BwManager.hpp"
#include "include/Logger.hpp"
#include "include/PerformanceCounters.hpp"
#include "include/MySharedMemory.hpp"
#include "include/PagePlacement.hpp"

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

/////////////////////////////////////////////
//provide this in a config
unsigned int _wait_start = 2;
unsigned int _num_polls = 20;
unsigned int _num_poll_outliers = 5;
useconds_t _poll_sleep = 200000;
double noise_allowed = 0.05;  // 5%
////////////////////////////////////////////

unsigned long time_diff(struct timeval* start, struct timeval* stop) {
  unsigned long sec_res = stop->tv_sec - start->tv_sec;
  unsigned long usec_res = stop->tv_usec - start->tv_usec;
  return 1000000 * sec_res + usec_res;
}

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

  } else {
    LINFO("At least provide 1 monitored core!");
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
    LDEBUG("Sorry, Weights have not been provided!");
    exit(EXIT_FAILURE);
  }

}

void start_bw_manager() {

  //hill_climbing_pmigration();
  hill_climbing_mba();

}

void hill_climbing_mba() {

  std::vector<double> prev_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> best_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> stall_rate(active_cpus);
  std::vector<double> interval_diff(active_cpus);
  std::vector<double> minimum_interference(active_cpus);

  int i;
  int j;

  //timing parameters
  struct timeval tstart, tend;
  unsigned long length;

  get_stall_rate();
  sleep(_wait_start);

  best_stall_rate.at(1) = 0.4033525365;
  LINFOF("Minimum allowable stall rate: %1.10lf", best_stall_rate.at(1))

  LINFO("Running the adaptive-co-scheduled scenario!");
  gettimeofday(&tstart, NULL);
  for (i = 100; i >= 10; i -= ADAPTATION_STEP) {

    LINFOF("Going to check an MBA of %d", i);
    if (i == 70 || i == 80)
      continue;
    //First check the stall rate of the initial weights without moving pages!
    char buf[32];
    sprintf(buf, "sudo pqos -e 'mba@0:0=%d'", i);
    system(buf);

    //Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    for (j = 0; j < active_cpus; j++) {

      //compute the minimum stall rate @ app
      interval_diff.at(j) = stall_rate.at(j) - prev_stall_rate.at(j);
      interval_diff.at(j) = round(interval_diff.at(j) * 100) / 100;
      minimum_interference.at(j) = (noise_allowed * prev_stall_rate.at(j));
      LINFOF(
          "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
          j, i, stall_rate.at(j), prev_stall_rate.at(j), best_stall_rate.at(j),
          interval_diff.at(j), minimum_interference.at(j));

      //best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
    }

    // Assume App 0 is memory intensive and App 1 is compute intensive
    // First check if we are hurting the performance of the compute intensive app upto a certain percentage (5%)
    if (interval_diff.at(1) > minimum_interference.at(1)) {
      LINFO(
          "Exceeded the Minimal allowable interference for App 1, continue climbing!");
    }

    else if (stall_rate.at(1) <= best_stall_rate.at(1) * 1.001
        || std::isnan(stall_rate.at(1))) {
      LINFO("Minimal allowable interference for App 1 achieved, stop climbing!");
      break;
    }

    else {
      LINFO("Performance improvement for App 1, continue climbing");
    }
    //At the end update previous stall rate to the current stall rate!
    for (j = 0; j < active_cpus; j++) {
      prev_stall_rate.at(j) = stall_rate.at(j);
    }

  }

  LINFO("My work here is done! Enjoy the speedup");
  gettimeofday(&tend, NULL);
  length = time_diff(&tstart, &tend);
  LINFOF("Adaptation concluded in %ldms\n", length / 1000);
}

void hill_climbing_pmigration() {
  //First read the memory segments to be moved
  std::vector<MySharedMemory> mem_segments = get_shared_memory();

  LINFOF("Number of Segments: %lu", mem_segments.size());

  //some sanity check
  if (mem_segments.size() == 0) {
    LINFO("No segments found! Exiting");
    destroy_shared_memory();
    stop_all_counters();
    exit(EXIT_FAILURE);
  }

  std::vector<double> prev_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> best_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> stall_rate(active_cpus);
  std::vector<double> interval_diff(active_cpus);
  std::vector<double> minimum_interference(active_cpus);

  double i;
  int j;
  bool terminate = false;

  //timing parameters
  struct timeval tstart, tend;
  unsigned long length;

  get_stall_rate();
  sleep(_wait_start);

  best_stall_rate.at(1) = 0.4033525365;
  LINFOF("Minimum allowable stall rate: %1.10lf", best_stall_rate.at(1))

  LINFO("Running the adaptive-co-scheduled scenario!");
  gettimeofday(&tstart, NULL);
  for (i = 0; !terminate; i += ADAPTATION_STEP) {

    if (i >= sum_nww) {
      i = sum_nww;
      terminate = true;
    }

    LINFOF("Going to check a ratio of %.2f", i);
    //First check the stall rate of the initial weights without moving pages!
    if (i != 0) {
      //stop_counters();
      place_all_pages(mem_segments, i);
      //start_counters();
    }

    //Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    for (j = 0; j < active_cpus; j++) {

      //compute the minimum stall rate @ app
      interval_diff.at(j) = stall_rate.at(j) - prev_stall_rate.at(j);
      interval_diff.at(j) = round(interval_diff.at(j) * 100) / 100;
      minimum_interference.at(j) = (noise_allowed * prev_stall_rate.at(j));
      LINFOF(
          "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
          j, i, stall_rate.at(j), prev_stall_rate.at(j), best_stall_rate.at(j),
          interval_diff.at(j), minimum_interference.at(j));

      //best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
    }

    // Assume App 0 is memory intensive and App 1 is compute intensive
    // First check if we are hurting the performance of the compute intensive app upto a certain percentage (5%)
    if (interval_diff.at(1) > minimum_interference.at(1)) {
      LINFO(
          "Exceeded the Minimal allowable interference for App 1, continue climbing!");
    }

    else if (stall_rate.at(1) <= best_stall_rate.at(1) * 1.001
        || std::isnan(stall_rate.at(1))) {
      LINFO("Minimal allowable interference for App 1 achieved, stop climbing!");
      break;
    }

    else {
      LINFO("Performance improvement for App 1, continue climbing");
    }
    //At the end update previous stall rate to the current stall rate!
    for (j = 0; j < active_cpus; j++) {
      prev_stall_rate.at(j) = stall_rate.at(j);
    }

  }

  LINFO("My work here is done! Enjoy the speedup");
  gettimeofday(&tend, NULL);
  length = time_diff(&tstart, &tend);
  LINFOF("Adaptation concluded in %ldms\n", length / 1000);
}

int main(int argc, char **argv) {

  // parse and display the configuration
  read_config();

  /* if (BWMAN_WORKERS == MAX_NODES) {
   LINFOF("No. of workers equals MAX_NODES (%d==%d)! Exiting", BWMAN_WORKERS,
   MAX_NODES);
   destroy_shared_memory();
   exit(EXIT_FAILURE);
   } */

  //set sum_ww & sum_nww & initialize the weights!
  get_sum_nww_ww(BWMAN_WORKERS);

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

void read_weights(char filename[]) {
  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;

  const char s[2] = ",";
  char *token;

  int j = 0;

  double weight;
  int id;

  fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Weights have not been provided!\n");
    exit(EXIT_FAILURE);
  }

  while ((read = getline(&line, &len, fp)) != -1) {
    char *strtok_saveptr;
    // printf("Retrieved line of length %zu :\n", read);
    //printf("%s", line);

    // get the first token
    token = strtok_r(line, s, &strtok_saveptr);
    weight = atof(token);
    //printf(" %s\n", token);

    // get the second token
    token = strtok_r(NULL, s, &strtok_saveptr);
    id = atoi(token);

    BWMAN_WEIGHTS.push_back(make_pair(weight, id));

    //printf(" %s\n", token);
    j++;
  }

  //sort the vector in ascending order
  sort(BWMAN_WEIGHTS.begin(), BWMAN_WEIGHTS.end());

  /*for (j = 0; j < MAX_NODES; j++) {
   printf("id: %d weight: %.2f\n", BWMAN_WEIGHTS.at(j).second,
   BWMAN_WEIGHTS.at(j).first);
   }*/

  fclose(fp);
  if (line)
    free(line);

  LINFO("weights initialized!");

  return;
}

void get_sum_nww_ww(int num_workers) {

  int i;

  if (num_workers == 1) {
    //workers: 0
    LDEBUG("Worker Nodes: 0");
    for (i = 0; i < MAX_NODES; i++) {
      if (BWMAN_WEIGHTS.at(i).second == 0) {
        //printf("id: %d weight: %.2f\n", BWMAN_WEIGHTS.at(i).second,
        //       BWMAN_WEIGHTS.at(i).first);
        sum_ww += BWMAN_WEIGHTS.at(i).first;
      } else {
        sum_nww += BWMAN_WEIGHTS.at(i).first;
      }
    }
  } else if (num_workers == 2) {
    //workers: 0,1
    LDEBUG("Worker Nodes: 0,1");
    for (i = 0; i < MAX_NODES; i++) {
      if (BWMAN_WEIGHTS.at(i).second == 0 || BWMAN_WEIGHTS.at(i).second == 1) {
        //printf("id: %d weight: %.2f\n", BWMAN_WEIGHTS.at(i).second,
        //       BWMAN_WEIGHTS.at(i).first);
        sum_ww += BWMAN_WEIGHTS.at(i).first;
      } else {
        sum_nww += BWMAN_WEIGHTS.at(i).first;
      }
    }
  } else if (num_workers == 3) {
    //workers: 1,2,3
    LDEBUG("Worker Nodes: 1,2,3");
    for (i = 0; i < MAX_NODES; i++) {
      if (BWMAN_WEIGHTS.at(i).second == 1 || BWMAN_WEIGHTS.at(i).second == 2
          || BWMAN_WEIGHTS.at(i).second == 3) {
        //printf("id: %d weight: %.2f\n", BWMAN_WEIGHTS.at(i).second,
        //       BWMAN_WEIGHTS.at(i).first);
        sum_ww += BWMAN_WEIGHTS.at(i).first;
      } else {
        sum_nww += BWMAN_WEIGHTS.at(i).first;
      }
    }
  } else if (num_workers == 4) {
    //workers: 0,1,2,3
    LDEBUG("Worker Nodes: 0,1,2,3");
    for (i = 0; i < MAX_NODES; i++) {
      if (BWMAN_WEIGHTS.at(i).second == 0 || BWMAN_WEIGHTS.at(i).second == 1
          || BWMAN_WEIGHTS.at(i).second == 2
          || BWMAN_WEIGHTS.at(i).second == 3) {
        //printf("id: %d weight: %.2f\n", BWMAN_WEIGHTS.at(i).second,
        //       BWMAN_WEIGHTS.at(i).first);
        sum_ww += BWMAN_WEIGHTS.at(i).first;
      } else {
        sum_nww += BWMAN_WEIGHTS.at(i).first;
      }
    }
  } else if (num_workers == 8) {
    //workers: all
    LDEBUG("Worker Nodes: All Nodes");
    for (i = 0; i < MAX_NODES; i++) {
      //printf("id: %d weight: %.2f\n", BWMAN_WEIGHTS.at(i).second,
      //       BWMAN_WEIGHTS.at(i).first);
      sum_ww += BWMAN_WEIGHTS.at(i).first;
    }
  } else {
    LDEBUGF("Sorry, %d workers is not supported at the moment!", num_workers);
    exit(EXIT_FAILURE);
  }

  if ((int) round((sum_nww + sum_ww)) != 100) {
    LDEBUGF(
        "Sum of WW and NWW must be equal to 100! WW=%.2f\tNWW=%.2f\tSUM=%.2f\n",
        sum_ww, sum_nww, sum_nww + sum_ww);
    exit(-1);
  } else {
    LDEBUGF("WW = %.2f\tNWW = %.2f\n", sum_ww, sum_nww);
  }

  return;
}

