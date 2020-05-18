#include "include/BwManager.hpp"

#include <stdlib.h>

#include <boost/program_options.hpp>
#include <cmath>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "include/Logger.hpp"
#include "include/MbaHandler.hpp"
#include "include/MySharedMemory.hpp"
#include "include/PagePlacement.hpp"
#include "include/PerformanceCounters.hpp"
#include "include/Utilities.hpp"

// number of workers
static bool OPT_NUM_WORKERS = false;
bool MONITORED_CORES = false;
// static bool WEIGHTS = false;
bool BWMAN_MODE = false;
bool FIXED_RATIO = false;

int BWMAN_WORKERS = 1;
// 0 - adaptive-coscheduled, 1 - fixed-ratio, 2 - adaptive-standalone
int bwman_mode_value = 0;
int fixed_ratio_value = 0;

static bool is_initialized = false;

// sum of worker nodes weights
double sum_ww = 0;
// sum of non-worker nodes weights
double sum_nww = 0;

using namespace std;
using namespace boost::program_options;

vector<int> BWMAN_CORES;
vector<pair<double, int>> BWMAN_WEIGHTS;
int active_cpus;

// const char* monitored_cores_s;
std::string monitored_cores_s;
std::string weights;
std::string strConfig = "";
double target_slo;
std::string server;
int port;

int current_remote_ratio;
int optimal_mba = 100;
double delta_hp;  // operational region of the controller (5%) - HP
double delta_be;  // operational region of the controller (5%) - BE

void read_config(int argc, const char *argv[]) {
  try {
    options_description generalOptions{"General Options"};
    generalOptions.add_options()("help,h", "Help screen")(
        "config,c",
        value<std::string>(&strConfig)->default_value("default.ini"),
        "name of the configuration file")(
        "BWMAN_MODE,m", value<int>(&bwman_mode_value)->default_value(0),
        "bwman mode value, 0=abc-numa, 1=pm-only, 2=mba-only, 3=linux-default, "
        "4=mba-10, 5=test")(
        "BWMAN_WEIGHTS,w",
        value<std::string>(&weights)->default_value("weights/weights_1w.txt"),
        "weights of BE application")(
        "BWMAN_CORES,d",
        value<std::string>(&monitored_cores_s)->default_value("0,10"),
        "bwman monitored cores")(
        "TARGET_SLO,t", value<double>(&target_slo)->default_value(1000),
        "target slo (99th percentile (usec))")(
        "TCP_SERVER,s",
        value<std::string>(&server)->default_value("146.193.41.52"),
        "tcp server for latency measurements")(
        "PORT,p", value<int>(&port)->default_value(1234), "tcp server port")(
        "REMOTE_RATIO,r", value<int>(&current_remote_ratio)->default_value(0),
        "current remote ratio")("DELTA_HP,o",
                                value<double>(&delta_hp)->default_value(0.5),
                                "HP operation region")(
        "DELTA_BE,b", value<double>(&delta_be)->default_value(0.001),
        "BE operation region");

    variables_map vm;
    store(parse_command_line(argc, argv, generalOptions), vm);
    notify(vm);

    if (vm.count("help")) {
      std::cout << generalOptions << '\n';
      exit(EXIT_SUCCESS);
    } else if (vm.count("config")) {
      LINFO("BWMAN PARAMETERS");
      LINFOF("BWMAN_MODE: %d", bwman_mode_value);
      LINFOF("BWMAN_WEIGHTS file: %s", weights.c_str());
      LINFOF("BWMAN_CORES: %s", monitored_cores_s.c_str());
      LINFOF("TARGET_SLO: %.0lf", target_slo);
      LINFOF("TCP_SERVER: %s", server.c_str());
      LINFOF("PORT: %d", port);
      LINFOF("REMOTE_RATIO: %d", current_remote_ratio);
      LINFOF("DELTA_HP: %.2lf", delta_hp);
      LINFOF("DELTA_BE: %.4lf", delta_be);
    }
  } catch (const error &ex) {
    std::cerr << ex.what() << '\n';
  }

  LINFOF("NUMA NODES: %d", MAX_NODES);

  OPT_NUM_WORKERS = getenv("BWMAN_WORKERS") != nullptr;
  if (OPT_NUM_WORKERS) {
    BWMAN_WORKERS = stoi(std::getenv("BWMAN_WORKERS"));
  }

  MONITORED_CORES = true;
  /* MONITORED_CORES = getenv("BWMAN_CORES") != nullptr;
  if (MONITORED_CORES) {
    monitored_cores_s = getenv("BWMAN_CORES");
    LINFOF("monitoring_core: %s", monitored_cores_s.c_str());
    */

  // tokenize the bwman_cores!
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
        "At least provide 2 monitoring cores (co-scheduled applications > "
        "2)");
    exit(EXIT_FAILURE);
  }

  /* } else {
     LINFO("At least provide 1 monitored core! e.g. BWMAN_CORES=0,10");
     exit(EXIT_FAILURE);
   }*/

  // check if the monitored cores vector is empty
  if (BWMAN_CORES.empty()) {
    LINFO("BWMAN_CORES vector is empty!");
    exit(EXIT_FAILURE);
  }

  /* BWMAN_MODE = getenv("BWMAN_MODE") != nullptr;
   if (BWMAN_MODE) {
     bwman_mode_value = stoi(getenv("BWMAN_MODE"));
   }
   LINFOF("BWMAN_MODE: %d", bwman_mode_value);
   */

  FIXED_RATIO = getenv("FIXED_RATIO") != nullptr;
  if (FIXED_RATIO) {
    fixed_ratio_value = stoi(getenv("FIXED_RATIO"));
  }
  LINFOF("FIXED_RATIO: %d", fixed_ratio_value);

  /* WEIGHTS = getenv("BWMAN_WEIGHTS") != nullptr;
   if (WEIGHTS) {
     weights = getenv("BWMAN_WEIGHTS");*/
  // read the weights
  read_weights(weights);
  /*} else {
    LDEBUG(
        "Sorry, Weights have not been provided! e.g. "
        "BWMAN_WEIGHTS=weights/weights_1.txt");
    exit(EXIT_FAILURE);
  }*/
}

void start_bw_manager() {
  // first make sure the sliding window has been set
  double cl = 0;
  LINFO("Setting up the sliding window");
  while (cl <= 33) {
    cl = get_percentile_latency();
  }
  LINFO("Sliding window has been set up!");
  // second start the measurements thread
  spawn_measurement_thread();
  LINFO("Measurements thread has been spawned!");
  // third read the memory segments to be moved
  // if (bwman_mode_value != 3) {
  get_memory_segments();
  //}

  switch (bwman_mode_value) {
    case 0:
      LINFO("Running the abc-numa mode!");
      abc_numa();
      break;
    case 1:
      LINFO("Running the page-migration-only mode!");
      page_migration_only();
      break;
    case 2:
      LINFO("Running mba-only mode!");
      mba_only();
      break;
    case 3:
      LINFO("Running the linux-default mode!");
      linux_default();
      break;
    case 4:
      LINFO("Running the mba_10 mode!");
      mba_10();
      break;
    case 5:
      LINFO("Running the abc-numa test mode!");
      bw_manager_test();
      break;
    default:
      LINFO("Invalid mode!");
      exit(EXIT_FAILURE);
      break;
  }
}

int main(int argc, const char *argv[]) {
  // register signal SIGINT and signal handler
  // and also a terminate handler incase we terminate midway
  signal(SIGINT, signalHandler);
  std::set_terminate(terminateHandler);

  // parse and display the configuration
  read_config(argc, argv);

  /* if (BWMAN_WORKERS == MAX_NODES) {
   LINFOF("No. of workers equals MAX_NODES (%d==%d)! Exiting", BWMAN_WORKERS,
   MAX_NODES);
   destroy_shared_memory();
   exit(EXIT_FAILURE);
   } */

  // set sum_ww & sum_nww & initialize the weights!
  // get_sum_nww_ww(BWMAN_WORKERS);
  // initialize likwid
  initialize_likwid();

  // initialize mba
  initialize_mba();

  is_initialized = true;
  LDEBUG("Initialized");

  start_bw_manager();

  // Destroy the shared memory be4 exiting!
  destroy_shared_memory();

  // stop all the counters
  stop_all_counters();
  LINFO("Finalized");

  return 0;
}
