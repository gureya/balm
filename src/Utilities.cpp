/*
 * Utilities.cpp
 *
 *  Created on: Jan 7, 2020
 *      Author: David Daharewa Gureya
 */

#include "include/Utilities.hpp"

#include "include/BwManager.hpp"
#include "include/Logger.hpp"
#include "include/MyLogger.hpp"
#include "include/MySharedMemory.hpp"
#include "include/PagePlacement.hpp"
#include "include/PerformanceCounters.hpp"

// for set precision
#include <iomanip>
#include <iostream>

// for boost asio
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

using boost::asio::ip::tcp;

/////////////////////////////////////////////
// provide this in a config
unsigned int _wait_start = 2;
unsigned int _num_polls = 20;
unsigned int _num_poll_outliers = 5;
useconds_t _poll_sleep = 200000;
double noise_allowed = 0.05;  // 5%
double delta_hp = 0.5;        // operational region of the controller (5%) - HP
double delta_be = 0.001;      // operational region of the controller (5%) - BE
double phase_change = 0.1;    // phase change value
////////////////////////////////////////////

/////////////////////////////////////////////
// some dynamic global variables
std::vector<double> stall_rate(active_cpus);
std::vector<double> prev_stall_rate(active_cpus);
std::vector<double> best_stall_rate(active_cpus);
double current_latency;
/////////////////////////////////////////////

// For Logging purposes
std::vector<MyLogger> my_logs;

static int run = 1;
static int sleeptime = 5;

enum { BE = 0, HP };

using namespace std;

void signalHandler(int signum) {
  LINFOF("Interrupt signal %d received", signum);
  // cleanup and close up stuff here
  // terminate program
  destroy_shared_memory();
  stop_all_counters();
  run = 0;
  exit(signum);
}

unsigned long time_diff(struct timeval* start, struct timeval* stop) {
  unsigned long sec_res = stop->tv_sec - start->tv_sec;
  unsigned long usec_res = stop->tv_usec - start->tv_usec;
  return 1000000 * sec_res + usec_res;
}

/*
 * Split the execution time of the controller in monitoring periods of length T,
 * during which we monitor the stall rate of HP and BE
 * abc_numa = page migration + mba
 *
 */
void abc_numa() {
  // First read the memory segments to be moved
  // Move this to a different function i.e., memory initializer
  std::vector<MySharedMemory> mem_segments = get_shared_memory();

  LINFOF("Number of Segments: %lu", mem_segments.size());

  // some sanity check
  if (mem_segments.size() == 0) {
    LINFO("No segments found! Exiting");
    destroy_shared_memory();
    stop_all_counters();
    exit(EXIT_FAILURE);
  }

  // double target_stall_rate;
  int iter = 0;

  int i;
  for (i = 0; i < active_cpus; i++) {
    prev_stall_rate.push_back(std::numeric_limits<double>::infinity());
    best_stall_rate.push_back(std::numeric_limits<double>::infinity());
  }
  LINFOF("INITIAL Stall rate values: best.BE - %.10lf, previous.BE - %.10lf",
         best_stall_rate.at(BE), prev_stall_rate.at(BE));

  while (run) {
    // TODO: this can be inside the loop or outside the loop!
    // TODO: Define the operation region of the controller
    LINFO("======================================================");
    LINFOF("Starting a new iteration: %d", iter);
    LINFO("------------------------------------------------------");
    // target_stall_rate = 0.5030852868;  // some fake value
    // target_stall_rate = get_target_stall_rate(current_remote_ratio);
    // LINFOF("Target SLO at this point: %.10lf", target_stall_rate);

    // Measure the stall_rate of the applications
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_percentile_latency();
    /*  if (!std::isnan(stall_rate.at(HP))
     && stall_rate.at(HP) >= target_stall_rate * (1 - delta_hp)
     && stall_rate.at(HP) <= target_stall_rate * (1 + delta_hp)) {
     LINFO("Nothing can be done (SLO within the operation region)");
     LINFOF("target: %.10lf, current: %.10lf", target_stall_rate,
     stall_rate.at(HP));
     }*/

    if (current_latency != 0 && current_latency > target_slo * (1 + delta_hp)) {
      LINFOF(
          "SLO has been violated (ABOVE operation region) target: %.0lf, "
          "current: %.0lf",
          target_slo, current_latency);

      if (current_remote_ratio != 0) {
        // Enforce MBA
        LINFO("------------------------------------------------------");
        optimal_mba = search_optimal_mba();

        // Enforce Lazy Page migration while releasing MBA
        while (optimal_mba != 100) {
          // apply page migration
          LINFO("------------------------------------------------------");
          current_remote_ratio = apply_pagemigration_rl(mem_segments);
          // release MBA
          LINFO("------------------------------------------------------");
          optimal_mba = release_mba();
        }

      } else {
        LINFO(
            "Nothing can be done about SLO violation (Change in workload!), "
            "Find new target SLO!");
        LINFOF("target: %.0lf, current: %.0lf", target_slo, current_latency);
      }

    }

    else {
      LINFOF(
          "SLO has NOT been violated (BELOW operation region) target: %.0lf, "
          "current: %.0lf",
          target_slo, current_latency);

      /*
       * Optimize page migration either way (local to remote and vice versa)!
       * if the current stall rate is smaller than the previous stall rate,
       * move pages from local to remote node
       * Otherwise the BE may have become less memory-intensive,
       * move pages from remote to local node
       *
       */
      LINFOF("BE current: %.10lf, BE best: %.10lf", stall_rate.at(BE),
             best_stall_rate.at(BE));

      if (stall_rate.at(BE) < best_stall_rate.at(BE) * (1 + delta_be)) {
        LINFO("------------------------------------------------------");
        current_remote_ratio = apply_pagemigration_lr(mem_segments);
      } else if (stall_rate.at(BE) > best_stall_rate.at(BE) * (1 - delta_be)) {
        LINFO("------------------------------------------------------");
        current_remote_ratio = apply_pagemigration_rl(mem_segments);
      } else {
        LINFO(
            "Nothing can be done (SLO within the operation region && No "
            "performance improvement for BE)");
      }
    }

    LINFOF("End of iteration: %d, sleeping for %d seconds", iter, sleeptime);
    LINFOF("current_remote_ratio: %d, optimal_mba: %d", current_remote_ratio,
           optimal_mba);
    iter++;

    print_logs();
    sleep(sleeptime);
  }
}

/*
 * page migration only
 *
 */
void page_migration_only() {
  // First read the memory segments to be moved
  std::vector<MySharedMemory> mem_segments = get_shared_memory();

  LINFOF("Number of Segments: %lu", mem_segments.size());

  // some sanity check
  if (mem_segments.size() == 0) {
    LINFO("No segments found! Exiting");
    destroy_shared_memory();
    stop_all_counters();
    exit(EXIT_FAILURE);
  }

  int iter = 0;

  int i;
  for (i = 0; i < active_cpus; i++) {
    prev_stall_rate.push_back(std::numeric_limits<double>::infinity());
    best_stall_rate.push_back(std::numeric_limits<double>::infinity());
  }
  LINFOF("INITIAL Stall rate values: best.BE - %.10lf, previous.BE - %.10lf",
         best_stall_rate.at(BE), prev_stall_rate.at(BE));

  while (run) {
    // TODO: this can be inside the loop or outside the loop!
    // TODO: Define the operation region of the controller
    LINFO("======================================================");
    LINFOF("Starting a new iteration: %d", iter);
    LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_percentile_latency();

    if (current_latency != 0 && current_latency > target_slo * (1 + delta_hp)) {
      LINFOF(
          "SLO has been violated (ABOVE operation region) target: %.0lf, "
          "current: %.0lf",
          target_slo, current_latency);

      // apply page migration
      LINFO("------------------------------------------------------");
      current_remote_ratio = apply_pagemigration_rl(mem_segments);
    }

    else {
      LINFOF(
          "SLO has NOT been violated (BELOW operation region) target: %.0lf, "
          "current: %.0lf",
          target_slo, current_latency);

      /*
       * Optimize page migration either way (local to remote and vice versa)!
       * if the current stall rate is smaller than the previous stall rate,
       * move pages from local to remote node
       * Otherwise the BE may have become less memory-intensive,
       * move pages from remote to local node
       *
       */

      /*
       * After a new iteraion check if the current stall rate is within the
       * optimal region
       */
      double diff = stall_rate.at(BE) - best_stall_rate.at(BE);
      LINFOF("BE current: %.10lf, BE best: %.10lf, diff: %.10lf",
             stall_rate.at(BE), best_stall_rate.at(BE), diff);

      if ((diff < -(delta_be) && diff > -(phase_change)) ||
          diff == -(std::numeric_limits<double>::infinity())) {
        current_remote_ratio = apply_pagemigration_lr(mem_segments);
      } else if ((diff > delta_be && diff < phase_change) ||
                 diff == -(std::numeric_limits<double>::infinity())) {
        current_remote_ratio = apply_pagemigration_rl_be(mem_segments);
      } else if ((diff > -(delta_be) && diff < delta_be) ||
                 diff == -(std::numeric_limits<double>::infinity())) {
        LINFOF(
            "Nothing can be done (SLO within the operation region && No "
            "performance improvement for BE), delta_be: %.10lf",
            diff);
      } else {
        LINFOF("Phase change detected, diff: %.10lf", diff);
        // reset the best ratio value!
        best_stall_rate.at(BE) = stall_rate.at(BE);
      }
    }

    LINFOF("End of iteration: %d, sleeping for %d seconds", iter, sleeptime);
    LINFOF("current_remote_ratio: %d, optimal_mba: %d", current_remote_ratio,
           optimal_mba);
    iter++;

    print_logs();
    sleep(sleeptime);
  }
}

/*
 * MBA only Mode
 *
 */
void mba_only() {
  // First read the memory segments to be moved
  std::vector<MySharedMemory> mem_segments = get_shared_memory();

  LINFOF("Number of Segments: %lu", mem_segments.size());

  // some sanity check
  if (mem_segments.size() == 0) {
    LINFO("No segments found! Exiting");
    destroy_shared_memory();
    stop_all_counters();
    exit(EXIT_FAILURE);
  }

  int iter = 0;

  int i;
  for (i = 0; i < active_cpus; i++) {
    prev_stall_rate.push_back(std::numeric_limits<double>::infinity());
    best_stall_rate.push_back(std::numeric_limits<double>::infinity());
  }
  LINFOF("INITIAL Stall rate values: best.BE - %.10lf, previous.BE - %.10lf",
         best_stall_rate.at(BE), prev_stall_rate.at(BE));

  while (run) {
    // TODO: this can be inside the loop or outside the loop!
    // TODO: Define the operation region of the controller
    LINFO("======================================================");
    LINFOF("Starting a new iteration: %d", iter);
    LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_percentile_latency();

    if (current_latency != 0 && current_latency > target_slo * (1 + delta_hp)) {
      LINFOF(
          "SLO has been violated (ABOVE operation region) target: %.0lf, "
          "current: %.0lf",
          target_slo, current_latency);

      if (current_remote_ratio != 0) {
        // Enforce MBA
        LINFO("------------------------------------------------------");
        optimal_mba = search_optimal_mba();

      } else {
        LINFO(
            "Nothing can be done about SLO violation (Change in workload!), "
            "Find new target SLO!");
        LINFOF("target: %.0lf, current: %.0lf", target_slo, current_latency);
      }

    }

    else {
      LINFOF(
          "SLO has NOT been violated (BELOW operation region) target: %.0lf, "
          "current: %.0lf",
          target_slo, current_latency);

      /*
       * Optimize page migration either way (local to remote and vice versa)!
       * if the current stall rate is smaller than the previous stall rate,
       * move pages from local to remote node
       * Otherwise the BE may have become less memory-intensive,
       * move pages from remote to local node
       *
       */
      LINFOF("BE current: %.10lf, BE best: %.10lf", stall_rate.at(BE),
             best_stall_rate.at(BE));

      // Release MBA
      while (optimal_mba != 100) {
        LINFO("------------------------------------------------------");
        optimal_mba = release_mba();
      }
    }

    LINFOF("End of iteration: %d, sleeping for %d seconds", iter, sleeptime);
    LINFOF("current_remote_ratio: %d, optimal_mba: %d", current_remote_ratio,
           optimal_mba);
    iter++;

    print_logs();
    sleep(sleeptime);
  }
}

/*
 * linux default Mode
 *
 */
void linux_default() {
  int iter = 0;

  while (run) {
    LINFO("======================================================");
    LINFOF("Starting a new iteration: %d", iter);
    LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_percentile_latency();

    std::string my_action = "iter-" + std::to_string(iter);
    my_logger(current_remote_ratio, optimal_mba, target_slo, current_latency,
              stall_rate.at(HP), stall_rate.at(BE), my_action);

    iter++;

    print_logs();
    sleep(sleeptime);
  }
}

/*
 * Get the target SLO of the high-priority task at any particular point in time
 * Set MBA to the minimum value and measure the current stall_rate
 * After measuring set MBA to the maximum value
 *
 * Handling memory-intensive applications:
 * You can use the STOP signal to pause a process, and CONT to resume its
 * execution: kill -STOP ${PID} kill -CONT ${PID}
 *
 */
double get_target_stall_rate() {
  double target_stall_rate;
  int min_mba = 10;
  int max_mba = 100;

  LINFO("Getting the target SLO for the HP");
  if (current_remote_ratio != 0) {
    apply_mba(min_mba);
  }

  // Measure the stall_rate of the applications
  stall_rate =
      get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

  target_stall_rate = stall_rate.at(HP);

  if (current_remote_ratio != 0) {
    apply_mba(max_mba);
  }

  return target_stall_rate;
}

/*
 * The functions below:
 * Measure the latency of the HPA application
 *
 */
double get_percentile_latency() {
  double service_time;

  LINFO("Getting the current percentile latency for the HP");

  // poll the latency of the applications
  service_time = connect_to_client();

  return service_time;
}

double connect_to_client() {
  double service_time = 0;

  try {
    // socket creation
    boost::system::error_code error;
    boost::asio::io_service io_service;
    tcp::socket socket(io_service);

    // connection
    std::cout << "[Client] Connecting to server..." << std::endl;
    socket.connect(
        tcp::endpoint(boost::asio::ip::address::from_string(server), port),
        error);

    for (;;) {
      boost::array<char, 128> buf;

      size_t len = socket.read_some(boost::asio::buffer(buf), error);

      if (error == boost::asio::error::eof)
        break;  // Connection closed cleanly by peer.
      else if (error)
        throw boost::system::system_error(error);  // Some other error.

      // std::cout.write(buf.data(), len);
      service_time = boost::lexical_cast<double>(buf.data());
    }
  } catch (std::exception& e) {
    LINFO("Problem connecting to the client");
    std::cerr << e.what() << std::endl;
    // exit(EXIT_FAILURE);
  }

  return service_time;
}

/*
 * Search the highest MBA that still meets the target SLO
 * Apply binary search to reduce the search space
 * Valid MBA states in our case: 100, 90, 60, 50, 40, 30, 20, 10
 * TODO: check for transient values
 *
 */

int search_optimal_mba() {
  int i;
  double progress;

  i = 40;
  int previous_mba = 40;

  while (i != 10 || i != 30 || i != 50 || i != 90) {
    if (i == 0) {
      LINFO("End of valid MBA states, breaking!");
      optimal_mba = previous_mba;
      break;
    }

    apply_mba(i);

    // Measure the stall_rate of the applications after enforcing MBA
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the current latency
    current_latency = get_percentile_latency();

    std::string my_action = "apply_mba-" + std::to_string(i);
    my_logger(current_remote_ratio, i, target_slo, current_latency,
              stall_rate.at(HP), stall_rate.at(BE), my_action);

    // sanity checker
    if (current_latency == 0) {
      LINFO("0 latency reported, revert to the previous state and break!");
      apply_mba(optimal_mba);
      break;
    }

    if (current_latency <= target_slo * (1 + delta_hp)) {
      LINFOF("SLO has been achieved: target: %.0lf, current: %.0lf", target_slo,
             current_latency);
      optimal_mba = i;
      break;
    }

    else {
      LINFOF("SLO has NOT been achieved:  target: %.0lf, current: %.0lf",
             target_slo, current_latency);
    }

    // progress = stall_rate.at(HP) - target_stall_rate;
    progress = current_latency - target_slo;
    LINFOF("Progress: %.2lf", progress);
    previous_mba = i;
    i = mba_binary_search(i, progress);
    optimal_mba = i;
  }

  LINFOF("Optimal MBA value: %d", optimal_mba);
  return optimal_mba;
}

/*
 * Page migrations from remote to local node (HP to BE nodes)
 * TODO: Handle transient cases
 */
int apply_pagemigration_rl(std::vector<MySharedMemory> mem_segments) {
  int i;
  // apply the next ratio immediately
  if (current_remote_ratio > 0) {
    current_remote_ratio -= ADAPTATION_STEP;
  }

  for (i = current_remote_ratio; i >= 0; i -= ADAPTATION_STEP) {
    LINFOF("Going to check a ratio of %d", i);
    place_all_pages(mem_segments, i);

    // Measure the stall_rate of the applications
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the current latency measurement
    current_latency = get_percentile_latency();

    // update the BE best stall rate
    best_stall_rate.at(BE) =
        std::min(best_stall_rate.at(BE), stall_rate.at(BE));

    std::string my_action = "apply_ratio-" + std::to_string(i);
    my_logger(current_remote_ratio, optimal_mba, target_slo, current_latency,
              stall_rate.at(HP), stall_rate.at(BE), my_action);

    // sanity check
    if (current_latency == 0) {
      LINFOF(
          "NAN HP latency (STOP page migration): target: %.0lf, current: %.0lf",
          target_slo, current_latency);
      current_remote_ratio = i;
      break;
    }

    if (current_latency <= target_slo * (1 + delta_hp)) {
      LINFOF(
          "SLO has been achieved (STOP page migration): target: %.0lf, "
          "current: %.0lf",
          target_slo, current_latency);
      current_remote_ratio = i;
      break;
    } else {
      LINFOF(
          "SLO has NOT been achieved (CONTINUE page migration): target: %.0lf, "
          "current: %.0lf",
          target_slo, current_latency);
      current_remote_ratio = i;
    }
  }

  LINFOF("Current remote ratio: %d", current_remote_ratio);
  return current_remote_ratio;
}

/*
 * Page migrations from remote to local node (HP to BE nodes)
 * considering only the optimization of BE
 * TODO: Handle transient cases
 */
int apply_pagemigration_rl_be(std::vector<MySharedMemory> mem_segments) {
  int i;
  // apply the next ratio immediately
  if (current_remote_ratio > 0) {
    current_remote_ratio -= ADAPTATION_STEP;
  }

  for (i = current_remote_ratio; i >= 0; i -= ADAPTATION_STEP) {
    LINFOF("Going to check a ratio of %d", i);
    place_all_pages(mem_segments, i);

    // Measure the stall_rate of the applications
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the current latency measurement
    current_latency = get_percentile_latency();

    // update the BE best stall rate
    best_stall_rate.at(BE) =
        std::min(best_stall_rate.at(BE), stall_rate.at(BE));

    std::string my_action = "apply_ratio-" + std::to_string(i);
    my_logger(current_remote_ratio, optimal_mba, target_slo, current_latency,
              stall_rate.at(HP), stall_rate.at(BE), my_action);

    // sanity check
    /*   if (current_latency == 0) {
         LINFOF(
             "NAN HP latency (STOP page migration): target: %.0lf, current:
       %.0lf", target_slo, current_latency); current_remote_ratio = i; break;
       }*/
    double diff = stall_rate.at(BE) - best_stall_rate.at(BE);

    if (diff > delta_be) {
      LINFOF(
          "page optimization achieved (STOP page migration): target: %.0lf, "
          "current: %.0lf, delta: %.10lf",
          target_slo, current_latency, diff);
      current_remote_ratio = i;
      break;
    } else {
      LINFOF(
          "page optimazation possible (CONTINUE page migration): target: "
          "%.0lf, "
          "current: %.0lf",
          target_slo, current_latency);
      current_remote_ratio = i;
    }
  }

  LINFOF("Current remote ratio: %d", current_remote_ratio);
  return current_remote_ratio;
}

/*
 * Page migrations from local to remote node (BE to HP nodes)
 * to optimize the BW of BE
 * TODO: Handle transient cases
 *
 */

int apply_pagemigration_lr(std::vector<MySharedMemory> mem_segments) {
  int i;
  // apply the next ratio immediately
  if (current_remote_ratio < 100) {
    current_remote_ratio += ADAPTATION_STEP;
  }

  for (i = current_remote_ratio; i <= 100; i += ADAPTATION_STEP) {
    LINFOF("Going to check a ratio of %d", i);
    if (i != 0) {
      place_all_pages(mem_segments, i);
    }

    // Measure the stall_rate of the applications
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the current latency measurement
    current_latency = get_percentile_latency();

    // update the BE best stall rate
    best_stall_rate.at(BE) =
        std::min(best_stall_rate.at(BE), stall_rate.at(BE));

    std::string my_action = "apply_ratio-" + std::to_string(i);
    my_logger(current_remote_ratio, optimal_mba, target_slo, current_latency,
              stall_rate.at(HP), stall_rate.at(BE), my_action);

    // First check if we are violating the SLO
    if (current_latency != 0 && current_latency > target_slo * (1 + delta_hp)) {
      LINFOF("SLO has been violated target: %.0lf, current(HP): %.0lf",
             target_slo, current_latency);
      LINFOF("current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf",
             stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE));
      if (i != 0) {
        LINFO("Going one step back before breaking!");
        place_all_pages(mem_segments, (i - ADAPTATION_STEP));
        current_remote_ratio = i - ADAPTATION_STEP;
      } else {
        current_remote_ratio = i;
      }
      break;
    }

    // then check if there is any performance improvement for BE
    else if (stall_rate.at(BE) > best_stall_rate.at(BE) * (1 + delta_be) ||
             std::isnan(stall_rate.at(BE))) {
      LINFO("No performance improvement for the BE");
      LINFOF("current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf",
             stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE));
      if (i != 0) {
        LINFO("Going one step back before breaking!");
        place_all_pages(mem_segments, (i - ADAPTATION_STEP));
        current_remote_ratio = i - ADAPTATION_STEP;
      } else {
        current_remote_ratio = i;
      }
      break;
    }

    // if performance improvement and no SLO violation continue climbing
    else {
      LINFO(
          "Performance improvement for BE without SLO Violation, continue "
          "climbing");
      LINFOF(
          "current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf, "
          "latency(HP): %.0lf",
          stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE),
          current_latency);
      current_remote_ratio = i;
    }
  }

  LINFOF("Current remote ratio: %d", current_remote_ratio);
  return current_remote_ratio;
}

/*
 * Releasing MBA after page migration
 * continue increasing the MBA value until the maximum value!
 *
 */
int release_mba() {
  int i;
  // apply the next mba immediately
  optimal_mba += 10;

  for (i = optimal_mba; i <= 100; i += 10) {
    if (i == 70 || i == 80) continue;

    apply_mba(i);
    // Measure the stall_rate of the applications
    stall_rate =
        get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the current latency
    current_latency = get_percentile_latency();

    std::string my_action = "apply_mba-" + std::to_string(i);
    my_logger(current_remote_ratio, i, target_slo, current_latency,
              stall_rate.at(HP), stall_rate.at(BE), my_action);

    if (current_latency != 0 && current_latency > target_slo * (1 + delta_hp) &&
        current_remote_ratio != 0) {
      LINFOF(
          "SLO violation has been detected (STOP releasing MBA): target: "
          "%.0lf, current: %.0lf",
          target_slo, current_latency);
      optimal_mba = i;
      break;
    } else {
      LINFOF(
          "SLO violation has NOT been detected (CONTINUE releasing MBA): "
          "target: %.0lf, current: %.0lf",
          target_slo, current_latency);
      optimal_mba = i;
    }
  }

  LINFOF("Optimal MBA: %d", optimal_mba);
  return optimal_mba;
}

/*
 * Apply a single MBA value
 * TODO: Avoid using a system call to do this, instead use the library directly!
 */
void apply_mba(int mba_value) {
  LINFOF("Applying MBA of %d", mba_value);
  char buf[32];
  sprintf(buf, "sudo pqos -e 'mba@0:0=%d'", mba_value);
  system(buf);
}

/*
 * Binary search in MBA
 */
int mba_binary_search(int current_mba, double progress) {
  int next_mba;
  if (progress > 0) {
    if (current_mba == 40)
      next_mba = 20;
    else if (current_mba == 20)
      next_mba = 10;
    else if (current_mba == 60)
      next_mba = 50;
    else
      next_mba = 0;

  } else if (progress < 0) {
    if (current_mba == 40)
      next_mba = 60;
    else if (current_mba == 20)
      next_mba = 30;
    else if (current_mba == 60)
      next_mba = 90;
    else
      next_mba = 0;
  }

  return next_mba;
}

/*
 * A function to test individual components of the project
 *
 */

void bw_manager_test() {
  // double target_stall_rate;

  // First read the memory segments to be moved
  std::vector<MySharedMemory> mem_segments = get_shared_memory();
  LINFOF("Number of Segments: %lu", mem_segments.size());

  // some sanity check
  if (mem_segments.size() == 0) {
    LINFO("No segments found! Exiting");
    destroy_shared_memory();
    stop_all_counters();
    exit(EXIT_FAILURE);
  }

  LINFO("==============================================");
  LINFO("TESTING get_target_stall_rate function");
  LINFO("----------------------------------------------");
  LINFOF("Target SLO at this point: %.0lf", target_slo);

  // Measure the stall_rate of the applications
  stall_rate =
      get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

  // Measure the latency measurement
  current_latency = get_percentile_latency();

  LINFOF("Stall rate: target: %.0lf, current: %.10lf, latency: %.0lf",
         target_slo, stall_rate.at(HP), current_latency);

  LINFO("==============================================");
  LINFO("TESTING search_optimal_mba function");
  LINFO("----------------------------------------------");
  optimal_mba = search_optimal_mba();

  LINFO("==============================================");
  LINFO("TESTING apply_pagemigration_lr function");
  LINFO("----------------------------------------------");
  current_remote_ratio = apply_pagemigration_lr(mem_segments);

  LINFO("==============================================");
  LINFO("TESTING apply_pagemigration_rl function");
  LINFO("----------------------------------------------");
  current_remote_ratio = apply_pagemigration_rl(mem_segments);

  LINFO("==============================================");
  LINFO("TESTING release_mba function");
  LINFO("----------------------------------------------");
  optimal_mba = release_mba();

  LINFO("==============================================");
  LINFOF("FINAL VALUES: current_optimal_mba: %d, current_remote_ratio: %d",
         optimal_mba, current_remote_ratio);
}

void find_optimal_lr_ratio() {
  // double target_stall_rate;

  // First read the memory segments to be moved
  std::vector<MySharedMemory> mem_segments = get_shared_memory();
  LINFOF("Number of Segments: %lu", mem_segments.size());

  // some sanity check
  if (mem_segments.size() == 0) {
    LINFO("No segments found! Exiting");
    destroy_shared_memory();
    stop_all_counters();
    exit(EXIT_FAILURE);
  }

  int i;
  for (i = 0; i < active_cpus; i++) {
    prev_stall_rate.push_back(std::numeric_limits<double>::infinity());
    best_stall_rate.push_back(std::numeric_limits<double>::infinity());
  }
  LINFOF("INITIAL Stall rate values: best.BE - %.10lf, previous.BE - %.10lf",
         best_stall_rate.at(BE), prev_stall_rate.at(BE));

  LINFO("==============================================");
  LINFO("Finding optimal Local-remote ratio for BE");
  LINFO("----------------------------------------------");
  current_remote_ratio = apply_pagemigration_lr(mem_segments);

  // print the logs
  print_logs();
}

/*
 * print the logs
 *
 */
void print_logs() {
  for (size_t j = 0; j < my_logs.size(); j++) {
    /*printf("%d\t%d\t%.10lf\t%.10lf\t%.10lf\t%s\n",
     my_logs.at(j).current_remote_ratio, my_logs.at(j).current_mba_level,
     my_logs.at(j).HPA_target_stall_rate, my_logs.at(j).HPA_stall_rate,
     my_logs.at(j).BEA_stall_rate, my_logs.at(j).action);*/

    std::cout << std::fixed;
    std::cout << std::setprecision(10);

    cout << my_logs.at(j).current_remote_ratio << "\t"
         << my_logs.at(j).current_mba_level << "\t"
         << (int)my_logs.at(j).HPA_target_slo << "\t"
         << "\t" << (int)my_logs.at(j).HPA_currency_latency << "\t"
         << my_logs.at(j).HPA_stall_rate << "\t" << my_logs.at(j).BEA_stall_rate
         << "\t" << my_logs.at(j).action << endl;
  }
}

/*
 * Log all the current information
 */
void my_logger(int crr, int cml, double hpt, double hcl, double hps, double bes,
               std::string action) {
  // Log all the current information:
  MyLogger mylogger(crr, cml, hpt, hcl, hps, bes, action);

  my_logs.push_back(mylogger);
}

void test_fixed_ratio() {
  // First read the memory segments to be moved
  std::vector<MySharedMemory> mem_segments = get_shared_memory();
  LINFOF("Number of Segments: %lu", mem_segments.size());

  // some sanity check
  if (mem_segments.size() == 0) {
    LINFO("No segments found! Exiting");
    destroy_shared_memory();
    stop_all_counters();
    exit(EXIT_FAILURE);
  }
  stall_rate =
      get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);
  LINFOF("Before: stall_rate(BE): %.10lf, stall_rate(HP): %.10lf",
         stall_rate.at(BE), stall_rate.at(HP));
  LINFOF("Going to check a ratio of %d", fixed_ratio_value);
  place_all_pages(mem_segments, fixed_ratio_value);
  stall_rate =
      get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);
  LINFOF("After: stall_rate(BE): %.10lf, stall_rate(HP): %.10lf",
         stall_rate.at(BE), stall_rate.at(HP));
}

void read_weights(std::string filename) {
  FILE* fp;
  char* line = NULL;
  size_t len = 0;
  ssize_t read;

  const char s[2] = ",";
  char* token;

  int j = 0;

  double weight;
  int id;

  fp = fopen(filename.c_str(), "r");
  if (fp == NULL) {
    printf("Weights have not been provided!\n");
    exit(EXIT_FAILURE);
  }

  while ((read = getline(&line, &len, fp)) != -1) {
    char* strtok_saveptr;
    // printf("Retrieved line of length %zu :\n", read);
    // printf("%s", line);

    // get the first token
    token = strtok_r(line, s, &strtok_saveptr);
    weight = atof(token);
    // printf(" %s\n", token);

    // get the second token
    token = strtok_r(NULL, s, &strtok_saveptr);
    id = atoi(token);

    BWMAN_WEIGHTS.push_back(make_pair(weight, id));

    // printf(" %s\n", token);
    j++;
  }

  // sort the vector in ascending order
  sort(BWMAN_WEIGHTS.begin(), BWMAN_WEIGHTS.end());

  for (j = 0; j < MAX_NODES; j++) {
    printf("id: %d weight: %.2f\n", BWMAN_WEIGHTS.at(j).second,
           BWMAN_WEIGHTS.at(j).first);
  }

  fclose(fp);
  if (line) free(line);

  LINFO("weights initialized!");

  return;
}

/*
 //Starting page migration from the local weights
 void hill_climbing_pmigration_v2() {
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

 //timing parameters
 struct timeval tstart, tend;
 unsigned long length;

 get_stall_rate();
 sleep(_wait_start);

 LINFO("Running the adaptive-co-scheduled scenario!");
 gettimeofday(&tstart, NULL);
 for (i = 0; i <= 100; i += ADAPTATION_STEP) {

 LINFOF("Going to check a ratio of %.2f", i);
 //First check the stall rate of the initial weights without moving pages!
 if (i != 0) {
 //stop_counters();
 //place_all_pages(mem_segments, i);
 //start_counters();
 }

 //Measure the stall_rate of the applications
 stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
 _num_poll_outliers);

 for (j = 0; j < active_cpus; j++) {

 //compute the minimum stall rate @ app
 // App 0: BE, App 1: HP
 interval_diff.at(j) = stall_rate.at(j) - prev_stall_rate.at(j);
 //interval_diff.at(j) = round(interval_diff.at(j) * 100) / 100;
 minimum_interference.at(j) = (noise_allowed * prev_stall_rate.at(j));
 LINFOF(
 "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff:
 %1.10lf noise: %1.10lf", j, i, stall_rate.at(j), prev_stall_rate.at(j),
 best_stall_rate.at(j), interval_diff.at(j), minimum_interference.at(j));

 best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }

 // Assume App 0 is memory intensive (Best-Effort) and App 1 is compute
 intensive (High-Priority)!
 // First check if we are hurting the performance of the compute intensive app
 upto a certain percentage (5%) if (interval_diff.at(1) >
 minimum_interference.at(1)) { LINFO("Hmm...Is this really an interference?")
 std::vector<double> confirm_stall_rate_0 = get_average_stall_rate(
 _num_polls * 2, _poll_sleep, _num_poll_outliers * 2);
 double interval_diff_0 = confirm_stall_rate_0.at(1)
 - prev_stall_rate.at(1);
 //interval_diff_0 = round(interval_diff_0 * 100) / 100;
 double minimum_interference_0 = (noise_allowed * prev_stall_rate.at(1));
 if (interval_diff_0 > minimum_interference_0) {
 LINFO("I guess so!");
 if (i != 0) {
 LINFO("Going one step back before breaking!");
 //place_all_pages(mem_segments, (i - ADAPTATION_STEP));
 LINFOF("Final Ratio: %.2f", (i - ADAPTATION_STEP));
 } else {
 LINFOF("Final Ratio: %.2f", i);
 }
 LINFO(
 "[Phase 1]: Exceeded the Minimal allowable interference for App 1, continue
 climbing!"); break;
 }
 }

 else if (stall_rate.at(0) > best_stall_rate.at(0) * 1.001
 || std::isnan(stall_rate.at(0))) {
 // just make sure that its not something transient...!
 LINFO("Hmm... Is this the best we can do?");
 std::vector<double> confirm_stall_rate = get_average_stall_rate(
 _num_polls * 2, _poll_sleep, _num_poll_outliers * 2);
 if (confirm_stall_rate.at(0) > best_stall_rate.at(0) * 1.001
 || std::isnan(confirm_stall_rate.at(0))) {
 LINFO("I guess so!");
 if (i != 0) {
 LINFO("Going one step back before breaking!");
 //place_all_pages(mem_segments, (i - ADAPTATION_STEP));
 LINFOF("Final Ratio: %.2f", (i - ADAPTATION_STEP));
 } else {
 LINFOF("Final Ratio: %.2f", i);
 }
 LINFO(
 "[Phase 2]: Minimal allowable interference for App 1 achieved, stop
 climbing!"); break;
 }
 }

 else {
 LINFO(
 "[Phase 1 & 2]: Performance improvement for App 0 without interfering App 1,
 continue climbing");
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

 while (run) {
 //Measure the stall_rate of the applications
 stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
 _num_poll_outliers);

 for (j = 0; j < active_cpus; j++) {
 //compute the minimum stall rate @ app
 // App 0: BE, App 1: HP
 interval_diff.at(j) = stall_rate.at(j) - prev_stall_rate.at(j);
 //interval_diff.at(j) = round(interval_diff.at(j) * 100) / 100;
 minimum_interference.at(j) = (noise_allowed * prev_stall_rate.at(j));
 LINFOF(
 "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff:
 %1.10lf noise: %1.10lf", j, i, stall_rate.at(j), prev_stall_rate.at(j),
 best_stall_rate.at(j), interval_diff.at(j), minimum_interference.at(j));

 best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }
 sleep(sleeptime);
 }

 }

 void hill_climbing_mba_10() {

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

 i = 10;
 LINFOF("Going to check an MBA of %d", i);
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
 "App: %d MBA Level: %d StallRate: %1.10lf (previous %1.10lf; best %1.10lf)
 diff: %1.10lf noise: %1.10lf", j, i, stall_rate.at(j), prev_stall_rate.at(j),
 best_stall_rate.at(j), interval_diff.at(j), minimum_interference.at(j));

 //best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }

 // Assume App 0 is memory intensive and App 1 is compute intensive
 // First check if we are hurting the performance of the compute intensive app
 upto a certain percentage (5%) if (interval_diff.at(1) >
 minimum_interference.at(1)) { LINFO( "Exceeded the Minimal allowable
 interference for App 1, continue climbing!");
 }

 else if (stall_rate.at(1) <= best_stall_rate.at(1) * 1.001
 || std::isnan(stall_rate.at(1))) {
 LINFO("Minimal allowable interference for App 1 achieved, stop climbing!");
 }

 else {
 LINFO("Performance improvement for App 1, continue climbing");
 }
 //At the end update previous stall rate to the current stall rate!
 for (j = 0; j < active_cpus; j++) {
 prev_stall_rate.at(j) = stall_rate.at(j);
 }

 LINFO("My work here is done! Enjoy the speedup");
 gettimeofday(&tend, NULL);
 length = time_diff(&tstart, &tend);
 LINFOF("Adaptation concluded in %ldms\n", length / 1000);
 }

 void hill_climbing_mba_sha() {

 std::vector<double> prev_stall_rate(active_cpus,
 std::numeric_limits<double>::infinity());
 std::vector<double> best_stall_rate(active_cpus,
 std::numeric_limits<double>::infinity());
 std::vector<double> stall_rate(active_cpus);
 std::vector<double> interval_diff(active_cpus);
 std::vector<double> minimum_interference(active_cpus);

 int i;
 int j;
 double progress;

 //valid MBA states: 100, 90, 60, 50, 40, 30, 20, 10

 //timing parameters
 struct timeval tstart, tend;
 unsigned long length;

 get_stall_rate();
 sleep(_wait_start);

 best_stall_rate.at(1) = 0.6654351065;
 LINFOF("Minimum allowable stall rate: %1.10lf", best_stall_rate.at(1))

 LINFO("Running the adaptive-co-scheduled scenario!");
 gettimeofday(&tstart, NULL);

 i = 40;
 do {

 if (i == 0) {
 LINFO("Invalid MBA state, breaking!");
 break;
 }

 LINFOF("Going to check an MBA of %d", i);
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
 "App: %d MBA level: %d StallRate: %1.10lf (previous %1.10lf; best %1.10lf)
 diff: %1.10lf noise: %1.10lf", j, i, stall_rate.at(j), prev_stall_rate.at(j),
 best_stall_rate.at(j), interval_diff.at(j), minimum_interference.at(j));

 //best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }

 // Assume App 0 is memory intensive and App 1 is compute intensive
 // First check if we are hurting the performance of the compute intensive app
 upto a certain percentage (5%) if (interval_diff.at(1) >
 minimum_interference.at(1)) { LINFO( "Exceeded the Minimal allowable
 interference for App 1, continue climbing!");
 }

 else if (stall_rate.at(1) <= best_stall_rate.at(1) * 1.001
 || std::isnan(stall_rate.at(1))) {
 LINFO("Minimal allowable interference for App 1 achieved, stop climbing!");
 }

 else {
 LINFO("Performance improvement for App 1, continue climbing");
 }
 //At the end update previous stall rate to the current stall rate!
 for (j = 0; j < active_cpus; j++) {
 prev_stall_rate.at(j) = stall_rate.at(j);
 }
 progress = stall_rate.at(1) - (best_stall_rate.at(1) * 1.001);
 LINFOF("Progress: %1.10lf", progress);
 i = mba_binary_search(i, progress);

 } while (i != 10 || i != 30 || i != 50 || i != 90);

 LINFO("My work here is done! Enjoy the speedup");
 gettimeofday(&tend, NULL);
 length = time_diff(&tstart, &tend);
 LINFOF("Adaptation concluded in %ldms\n", length / 1000);
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

 best_stall_rate.at(1) = 0.6654351065;
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
 "App: %d MBA level: %d StallRate: %1.10lf (previous %1.10lf; best %1.10lf)
 diff: %1.10lf noise: %1.10lf", j, i, stall_rate.at(j), prev_stall_rate.at(j),
 best_stall_rate.at(j), interval_diff.at(j), minimum_interference.at(j));

 //best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }

 // Assume App 0 is memory intensive and App 1 is compute intensive
 // First check if we are hurting the performance of the compute intensive app
 upto a certain percentage (5%) if (interval_diff.at(1) >
 minimum_interference.at(1)) { LINFO( "Exceeded the Minimal allowable
 interference for App 1, continue climbing!");
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

 //Starting page migration from the canonical weights!
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

 best_stall_rate.at(1) = 0.6654351065;
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
 "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff:
 %1.10lf noise: %1.10lf", j, i, stall_rate.at(j), prev_stall_rate.at(j),
 best_stall_rate.at(j), interval_diff.at(j), minimum_interference.at(j));

 best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }

 // Assume App 0 is memory intensive (Best-Effort) and App 1 is compute
 intensive (High-Priority)!
 // First check if we are hurting the performance of the compute intensive app
 upto a certain percentage (5%) if (interval_diff.at(1) >
 minimum_interference.at(1)) { LINFO( "Exceeded the Minimal allowable
 interference for App 1, continue climbing!");
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

 void hill_climbing_pmigration_100() {
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

 //timing parameters
 struct timeval tstart, tend;
 unsigned long length;

 get_stall_rate();
 sleep(_wait_start);

 best_stall_rate.at(1) = 0.6654351065;
 LINFOF("Minimum allowable stall rate: %1.10lf", best_stall_rate.at(1))

 LINFO("Running the adaptive-co-scheduled scenario!");
 gettimeofday(&tstart, NULL);

 i = sum_nww;

 LINFOF("Going to check a ratio of %.2f", i);
 //First check the stall rate of the initial weights without moving pages!
 //stop_counters();
 place_all_pages(mem_segments, i);
 //start_counters();

 //Measure the stall_rate of the applications
 stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
 _num_poll_outliers);

 for (j = 0; j < active_cpus; j++) {

 //compute the minimum stall rate @ app
 interval_diff.at(j) = stall_rate.at(j) - prev_stall_rate.at(j);
 interval_diff.at(j) = round(interval_diff.at(j) * 100) / 100;
 minimum_interference.at(j) = (noise_allowed * prev_stall_rate.at(j));
 LINFOF(
 "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff:
 %1.10lf noise: %1.10lf", j, i, stall_rate.at(j), prev_stall_rate.at(j),
 best_stall_rate.at(j), interval_diff.at(j), minimum_interference.at(j));

 //best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }

 // Assume App 0 is memory intensive and App 1 is compute intensive
 // First check if we are hurting the performance of the compute intensive app
 upto a certain percentage (5%) if (interval_diff.at(1) >
 minimum_interference.at(1)) { LINFO( "Exceeded the Minimal allowable
 interference for App 1, continue climbing!");
 }

 else if (stall_rate.at(1) <= best_stall_rate.at(1) * 1.001
 || std::isnan(stall_rate.at(1))) {
 LINFO("Minimal allowable interference for App 1 achieved, stop climbing!");
 }

 else {
 LINFO("Performance improvement for App 1, continue climbing");
 }
 //At the end update previous stall rate to the current stall rate!
 for (j = 0; j < active_cpus; j++) {
 prev_stall_rate.at(j) = stall_rate.at(j);
 }

 LINFO("My work here is done! Enjoy the speedup");
 gettimeofday(&tend, NULL);
 length = time_diff(&tstart, &tend);
 LINFOF("Adaptation concluded in %ldms\n", length / 1000);
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
 */
