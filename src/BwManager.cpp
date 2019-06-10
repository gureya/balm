#include <iostream>
#include <sstream>

#include "include/BwManager.hpp"
#include "include/Logger.hpp"
#include "include/PerformanceCounters.hpp"
#include "include/MySharedMemory.hpp"
#include "include/PagePlacement.hpp"

bool MONITORED_CORES = false;
int WORKER_NODE = 1;

static bool is_initialized = false;

using namespace std;

vector<int> BWMAN_CORES;
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

void read_config(void) {

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

    int i;
    cout << "BWMAN_CORES: \t";
    for (i = 0; i < BWMAN_CORES.size(); i++) {
      cout << BWMAN_CORES.at(i) << "\t";
    }
    cout << endl;

    active_cpus = BWMAN_CORES.size();

  }

  else {
    LINFO("At least provide 1 monitored core!");
    exit(1);
  }

  //check is the monitored cores vector is empty
  if (BWMAN_CORES.empty()) {
    LINFO("BWMAN_CORES vector is empty!");
    exit(1);
  }

}

void start_bw_manager() {

  //First read the memory segments to be moved
  std::vector<MySharedMemory> mem_segments = get_shared_memory();

  LINFOF("Number of Segments: %lu", mem_segments.size());

  /*for (int i = 0; i < mem_segments.size(); i++) {
   printf(
   "processID: %d [PageAlignedStartAddress: %p PageAlignedLength: %lu PageCount: %lu] \n",
   mem_segments.at(i).processID,
   mem_segments.at(i).pageAlignedStartAddress,
   mem_segments.at(i).pageAlignedLength,
   mem_segments.at(i).pageAlignedLength / 4096);
   }*/

  std::vector<double> prev_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> best_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> stall_rate(active_cpus);
  std::vector<double> interval_diff(active_cpus);
  std::vector<double> minimum_interference(active_cpus);

  int i, j;

  for (i = 0; i <= 100; i += ADAPTATION_STEP) {

    LINFOF("Going to check a ratio of %d", i);

    //First check the stall rate of the initial weights without moving pages!
    if (i != 0) {
      place_all_pages(mem_segments, i);
    }

    //Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    for (j = 0; j < active_cpus; j++) {

      //compute the minimum stall rate @ app
      interval_diff.at(j) = stall_rate.at(j) - prev_stall_rate.at(j);
      //interval_diff.at(j) = round(interval_diff.at(j) * 100) / 100;
      minimum_interference.at(j) = (noise_allowed * prev_stall_rate.at(j));
      LINFOF(
          "App: %d Ratio: %d StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
          j, i, stall_rate.at(j), prev_stall_rate.at(j), best_stall_rate.at(j),
          interval_diff.at(j), minimum_interference.at(j));

      best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
    }

    // Assume App 0 is memory intensive and App 1 is compute intensive
    // First check if we are hurting the performance of the compute intensive app upto a certain percentage (5%)

    if (interval_diff.at(1) > minimum_interference.at(1)) {
      LINFO("Exceeded the Minimal allowable interference, Going one step back!");
      //before stopping go one step back and break
      place_all_pages(mem_segments, (i - ADAPTATION_STEP));
      LINFOF("Final Ratio: %d", (i - ADAPTATION_STEP));
      break;
    }

    else if (stall_rate.at(0) > best_stall_rate.at(0)) {
      LINFO("Performance degradation: Going one step back before breaking!");
      //before stopping go one step back and break
      place_all_pages(mem_segments, (i - ADAPTATION_STEP));
      LINFOF("Final Ratio: %d", (i - ADAPTATION_STEP));
      break;
    }

    else {
      //continue climbing!!
    }

    //At the end update previous stall rate to the current stall rate!
    for (j = 0; j < active_cpus; j++) {
      prev_stall_rate.at(j) = stall_rate.at(j);
    }

  }

  LINFO("My work here is done! Enjoy the speedup");

  //Destroy the shared memory be4 exiting!
  destroy_shared_memory();
}

int main(int argc, char **argv) {

  // parse and display the configuration
  read_config();

  // initialize likwid
  initialize_likwid();

  is_initialized = true;
  LDEBUG("Initialized");

  start_bw_manager();

  // stop all the counters
  stop_all_counters();
  LINFO("Finalized");

  return 0;
}

