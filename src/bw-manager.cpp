#include <iostream>
#include <sstream>

#include "include/bw-manager.hpp"
#include "include/performancecounters.hpp"
#include "include/Logger.hpp"

bool MONITORED_CORES = false;

static bool is_initialized = false;

using namespace std;

vector<int> MONITORED_CORES_VALUE;

const char* monitored_cores_s;

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
      MONITORED_CORES_VALUE.push_back(stoi(tok));
    }

    int i;
    for (i = 0; i < MONITORED_CORES_VALUE.size(); i++)
      cout << MONITORED_CORES_VALUE.at(i) << endl;

  }

  else {
    LINFO("At least provide 1 monitored core!");
    exit(1);
  }

}

int main(int argc, char **argv) {

  // parse and display the configuration
  read_config();

  // initialize likwid
  initialize_likwid();

  is_initialized = true;
  LDEBUG("Initialized");

  // stop all the counters
  stop_all_counters();
  LINFO("Finalized");

  return 0;
}
