#include <iostream>
#include <sstream>

#include "include/bw-manager.hpp"
#include "include/performancecounters.hpp"
#include "include/Logger.hpp"

bool MONITORED_CORES = false;

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

  std::vector<double> prev_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> best_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> stall_rate(active_cpus);

  int i, j;

  for (i = 0; i <= 100; i += ADAPTATION_STEP) {
    LINFOF("Going to check a ratio of %d", i);
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    for (j = 0; j < active_cpus; j++) {
      LINFOF("Average Stall rate for App %d: %1.10lf\n", j, stall_rate.at(j));
    }
  }

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

