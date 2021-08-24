/*
 * Utilities.cpp
 *
 *  Created on: Jan 7, 2020
 *      Author: David Daharewa Gureya
 */

#include "include/Utilities.hpp"

#include "include/BwManager.hpp"
#include "include/Logger.hpp"
#include "include/MbaHandler.hpp"
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
#include <thread>

using boost::asio::ip::tcp;

/////////////////////////////////////////////
// provide this in a config
unsigned int _wait_start = 2;
unsigned int _num_polls = 20;
unsigned int _num_poll_outliers = 5;
useconds_t _poll_sleep = 200000;
double noise_allowed = 0.05;  // 5%
double phase_change = 0.1;    // phase change value
bool optimization_complete = false;
////////////////////////////////////////////

/////////////////////////////////////////////
// some dynamic global variables
std::vector<double> stall_rate(active_cpus);
std::vector<double> prev_stall_rate(active_cpus);
std::vector<double> best_stall_rate(active_cpus);
std::vector<double> percentile_samples;
int violations_counter_f = 0;
int violations_counter_t = 0;
// same metrics for other xapian - hard-coded for now!
std::vector<double> percentile_samples_xpn;
int vlts_cnt_f = 0;
int vlts_cnt_t = 0;
// handle multiple LCAs, for now just 2
// TODO: Make this dynamic
// 0 - memcache, 1 - xapian
double current_latency;
double current_latency_xpn;

// started using slack variable for now!
double slack_up = 0.05;
double slack_down_mba = 0.2;
double slack_down_pg = 0.1;
double slack;
double slack_xpn;
/////////////////////////////////////////////

// For Logging purposes
std::vector<MyLogger> my_logs;

static int run = 1;
// static int sleeptime = 1;
useconds_t sleeptime = 20000;
int logCounter = 0;

enum {
  BE = 0,
  HP
};

using namespace std;

void signalHandler(int signum) {
  LINFOF("Interrupt signal %d received", signum);
  // cleanup and close up stuff here
  // terminate program
  destroy_shared_memory();
  // stop_all_counters();
  reset_mba();
  // print_logs();
  print_logs_v2();
  print_to_file();
  run = 0;
  exit(signum);
}

void terminateHandler() {
  LINFO("Terminate signal received!");
  // cleanup and close up stuff here
  // terminate program
  destroy_shared_memory();
  // stop_all_counters();
  reset_mba();
  // print_logs();
  print_logs_v2();
  print_to_file();
  run = 0;
  exit(EXIT_FAILURE);
}

unsigned long time_diff(struct timeval *start, struct timeval *stop) {
  unsigned long sec_res = stop->tv_sec - start->tv_sec;
  unsigned long usec_res = stop->tv_usec - start->tv_usec;
  return 1000000 * sec_res + usec_res;
}

// Get the memory segments of the BE
std::vector<MySharedMemory> mem_segments;

// just a counter to keep track of counting
int iter = 0;

void get_memory_segments() {
  // First read the memory segments to be moved
  LINFO("Waiting for the memory segments from BE");
  mem_segments = get_shared_memory();

  LINFOF("Number of Segments: %lu", mem_segments.size());
  //in case of mg.c.x
  /*sleep(1);
   LINFO("Enforce initial weighted interleaving for BE if not enforce");
   place_all_pages(mem_segments, current_remote_ratio);
   sleep(3);*/

  // some sanity check
  if (mem_segments.size() == 0) {
    LINFO("No segments found! Exiting");
    destroy_shared_memory();
    stop_all_counters();
    exit(EXIT_FAILURE);
  }

  // Initialize the best and previuos stall rates
  int i;
  for (i = 0; i < active_cpus; i++) {
    prev_stall_rate.push_back(std::numeric_limits<double>::infinity());
    best_stall_rate.push_back(std::numeric_limits<double>::infinity());
    stall_rate.push_back(std::numeric_limits<double>::infinity());
  }
  LINFOF("INITIAL Stall rate values: best.BE - %.10lf, previous.BE - %.10lf",
         best_stall_rate.at(BE), prev_stall_rate.at(BE));
}

/*
 * Split the execution time of the controller in monitoring periods of length T,
 * during which we monitor the stall rate of HP and BE
 * abc_numa = page migration + mba
 *
 */
void abc_numa() {
  LINFOF("Monitoring period: %d ms", sleeptime);
  while (run) {
    // TODO: this can be inside the loop or outside the loop!
    // TODO: Define the operation region of the controller
    // LINFO("======================================================");
    // LINFOF("Starting a new iteration: %d", iter);
    // LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    // stall_rate =
    //     get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_latest_percentile_latency();
    slack = (target_slo - current_latency) / target_slo;

    // for xapian, TODO: Make this dynamic
    current_latency_xpn = get_latest_percentile_latency_xpn();
    slack_xpn = (target_slo_xapian - current_latency_xpn) / target_slo_xapian;
    // update the BE best stall rate
    //  best_stall_rate.at(BE) =
    //      std::min(best_stall_rate.at(BE), stall_rate.at(BE));

    // log the measurements for the debugging purposes!
    std::string my_action = "iteration-" + std::to_string(iter);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    if (slack < slack_up || slack_xpn < slack_up) {
      /* if (current_latency != 0 && current_latency > target_slo * (1 +
       delta_hp)) {
       LINFOF(
       "SLO has been violated (ABOVE operation region) target: %.0lf, "
       "current: %.0lf",
       target_slo, current_latency);*/

      LINFOF(
          "SLO is about to be violated, slack: %.2lf, target: %.0lf, current: " "%.0lf, slack_xpn: %.2lf, target_xpn: %.2lf, current_xpn: %.2lf",
          slack, target_slo, current_latency, slack_xpn, target_slo_xapian,
          current_latency_xpn);

      // incase of single-skt check 100 also!
      // if (current_remote_ratio != 100) {
      if (current_remote_ratio != 0) {
        // Enforce MBA
        LINFO("------------------------------------------------------");
        // optimal_mba = search_optimal_mba();
        if (optimal_mba != 10) {
          apply_mba(10);
          optimal_mba = 10;
          sleep(3);
          // sleep for 1 sec
          // usleep(sleeptime);
          // usleep(500000);
          // log the measurements for the debugging purposes!

          my_action = "apply_mba-" + std::to_string(10);
          my_logger(chrono::system_clock::now(), current_remote_ratio,
                    optimal_mba, target_slo, current_latency, slack,
                    stall_rate.at(HP), stall_rate.at(BE), my_action,
                    logCounter++);
        }
        // Enforce Lazy Page migration while releasing MBA
        //  while (mba_flag) {
        while (optimal_mba != 100) {
          // evaluate SLO function whenever we come back here again!
          current_latency = get_latest_percentile_latency();
          slack = (target_slo - current_latency) / target_slo;

          // for xapian, TODO: Make this dynamic
          current_latency_xpn = get_latest_percentile_latency_xpn();
          slack_xpn = (target_slo_xapian - current_latency_xpn)
              / target_slo_xapian;
          // apply page migration if mba_10 didn't fix the violation
          // if (slack > slack_down_pg) {
          // LINFO("------------------------------------------------------");
          // current_remote_ratio = apply_pagemigration_rl();
          // current_remote_ratio = apply_pagemigration_lr_same_socket();
          //}
          // release MBA, only if we are below the operation region
          if (slack > slack_down_mba && slack_xpn > slack_down_mba) {
            LINFO("------------------------------------------------------");
            optimal_mba = release_mba();
          }
          if (slack > slack_up && slack < slack_down_mba) {
            // do nothing, we are in a safe but not in green zone, don't change
            // this config
          }
          if (slack < slack_up) {
            // danger zone, apply page migration
            /*if (optimal_mba != 10) {
             // apply mba_10 immediately
             apply_mba(10);
             optimal_mba = 10;
             sleep(3);
             // update the latencies
             current_latency = get_latest_percentile_latency();
             slack = (target_slo - current_latency) / target_slo;
             // for xapian, TODO: Make this dynamic
             current_latency_xpn = get_latest_percentile_latency_xpn();
             slack_xpn =
             (target_slo_xapian - current_latency_xpn) / target_slo_xapian;
             } else {*/
            current_remote_ratio = apply_pagemigration_lr_same_socket();
            //}
          }
        }
      } else {
        LINFO("Nothing can be done about SLO violation (Change in workload!), "
              "Find new target SLO!");
        LINFOF("target: %.0lf, current: %.0lf", target_slo, current_latency);
      }
      // }
    } /*else if (slack > slack_down && current_remote_ratio < 10) {
     LINFOF(
     "SLO has NOT been violated (BELOW operation region) target: %.0lf, "
     "current: %.0lf, slack: %.2lf",
     target_slo, current_latency, slack);
     current_remote_ratio = apply_pagemigration_lr();
     }*/

    /*  else {
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
    /*    double diff = stall_rate.at(BE) - best_stall_rate.at(BE);
     LINFOF("BE current: %.10lf, BE best: %.10lf, diff: %.10lf",
     stall_rate.at(BE), best_stall_rate.at(BE), diff);

     if (abs(diff) > phase_change &&
     abs(diff) != std::numeric_limits<double>::infinity()) {
     optimization_complete = false;
     LINFOF("Phase change detected, diff: %.10lf", diff);
     // reset the best ratio value!
     best_stall_rate.at(BE) = std::numeric_limits<double>::infinity();
     // fix this
     // check the direction of the optimization process!
     int direction = check_opt_direction();

     if (direction == 1) {
     current_remote_ratio = apply_pagemigration_rl_be();
     } else {
     current_remote_ratio = apply_pagemigration_lr();
     }
     } else {
     if (!optimization_complete) {
     if ((diff < -(delta_be)) ||
     diff == -(std::numeric_limits<double>::infinity()) || diff == 0)
     { current_remote_ratio = apply_pagemigration_lr(); } else if ((diff >
     delta_be) || diff == -(std::numeric_limits<double>::infinity())) {
     current_remote_ratio = apply_pagemigration_rl_be();
     } else if ((diff != 0 && diff > -(delta_be) && diff < delta_be) ||
     diff == -(std::numeric_limits<double>::infinity())) {
     LINFOF(
     "Nothing can be done (SLO within the operation region && No "
     "performance improvement for BE), delta_be: %.10lf",
     diff);
     // update the BE best stall rate
     best_stall_rate.at(BE) =
     std::min(best_stall_rate.at(BE), stall_rate.at(BE));
     } else {
     LINFO("Something else happened");
     exit(EXIT_FAILURE);
     }
     }
     }
     }

     LINFO("OPTIMIZATION COMPLETED!");
     optimization_complete = true;*/

    // LINFOF("End of iteration: %d, sleeping for %d seconds", iter, sleeptime);
    // LINFOF("current_remote_ratio: %d, optimal_mba: %d, current_latency:
    // %.0lf, slack: %.2lf", current_remote_ratio, optimal_mba, current_latency,
    // slack);
    iter++;

    // print_logs();
    usleep(sleeptime);
  }
}

/*
 * page migration only
 *
 */
void page_migration_only() {
  LINFOF("Monitoring period: %d ms", sleeptime);
  while (run) {
    // TODO: this can be inside the loop or outside the loop!
    // TODO: Define the operation region of the controller
    // LINFO("======================================================");
    // LINFOF("Starting a new iteration: %d", iter);
    // LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    // stall_rate =
    //     get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_latest_percentile_latency();
    slack = (target_slo - current_latency) / target_slo;

    // for xapian, TODO: Make this dynamic
    current_latency_xpn = get_latest_percentile_latency_xpn();
    slack_xpn = (target_slo_xapian - current_latency_xpn) / target_slo_xapian;
    // update the BE best stall rate
    // best_stall_rate.at(BE) =
    //    std::min(best_stall_rate.at(BE), stall_rate.at(BE));

    // log the measurements for the debugging purposes!
    std::string my_action = "iteration-" + std::to_string(iter);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    if (slack < slack_up || slack_xpn < slack_up) {
      //  if (current_latency != 0 && current_latency > target_slo * (1 +
      //  delta_hp)) {
      if (current_remote_ratio != 100) {
        // if (current_remote_ratio != 0) {
        LINFOF(
            "SLO has been violated, slack: %.2lf, target: %.0lf, " "current: %.0lf",
            slack, target_slo, current_latency);

        // apply page migration
        LINFO("------------------------------------------------------");
        // current_remote_ratio = apply_pagemigration_rl();
        current_remote_ratio = apply_pagemigration_lr_same_socket();
      } else {
        /*LINFO(
         "Nothing can be done about SLO violation (Change in workload!), "
         "Find new target SLO!");
         LINFOF("target: %.0lf, current: %.0lf", target_slo, current_latency);*/
      }
    }

    /*  else {
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
    /*    double diff = stall_rate.at(BE) - best_stall_rate.at(BE);
     LINFOF("BE current: %.10lf, BE best: %.10lf, diff: %.10lf",
     stall_rate.at(BE), best_stall_rate.at(BE), diff);

     if (abs(diff) > phase_change &&
     abs(diff) != std::numeric_limits<double>::infinity()) {
     optimization_complete = false;
     LINFOF("Phase change detected, diff: %.10lf", diff);
     // reset the best ratio value!
     best_stall_rate.at(BE) = std::numeric_limits<double>::infinity();
     // fix this
     // check the direction of the optimization process!
     int direction = check_opt_direction();

     if (direction == 1) {
     current_remote_ratio = apply_pagemigration_rl_be();
     } else {
     current_remote_ratio = apply_pagemigration_lr();
     }
     } else {
     if (!optimization_complete) {
     if ((diff < -(delta_be)) ||
     diff == -(std::numeric_limits<double>::infinity()) || diff == 0)
     { current_remote_ratio = apply_pagemigration_lr(); } else if ((diff >
     delta_be) || diff == -(std::numeric_limits<double>::infinity())) {
     current_remote_ratio = apply_pagemigration_rl_be();
     } else if ((diff > -(delta_be) && diff < delta_be) ||
     diff == -(std::numeric_limits<double>::infinity())) {
     LINFOF(
     "Nothing can be done (SLO within the operation region && No "
     "performance improvement for BE), delta_be: %.10lf",
     diff);
     // update the BE best stall rate
     best_stall_rate.at(BE) =
     std::min(best_stall_rate.at(BE), stall_rate.at(BE));
     } else {
     LINFO("Something else happened");
     exit(EXIT_FAILURE);
     }
     }
     }
     }

     LINFO("OPTIMIZATION COMPLETED!");
     optimization_complete = true;*/

    /*  LINFOF("End of iteration: %d, sleeping for %d seconds", iter,
     sleeptime); LINFOF( "current_remote_ratio: %d, optimal_mba: %d,
     current_latency: %.0lf, " "slack: %.2lf", current_remote_ratio,
     optimal_mba, current_latency, slack);*/
    iter++;

    // print_logs();
    usleep(sleeptime);
  }
}

int check_opt_direction() {
  // apply the next ratio immediately
  double tried_ratio = current_remote_ratio;

  // 0 = lr, 1 = rl
  int direction = 0;
  double tried_diff;
  std::vector<double> rl_str(active_cpus);
  if (tried_ratio > 0) {
    // first try from remote to local
    tried_ratio -= ADAPTATION_STEP;
    place_all_pages(mem_segments, tried_ratio);
    rl_str = get_average_stall_rate(_num_polls, _poll_sleep,
                                    _num_poll_outliers);

    // measure the difference
    tried_diff = stall_rate.at(BE) - rl_str.at(BE);
    if (tried_diff > 0) {
      direction = 1;                       // remote to local migration
      current_remote_ratio = tried_ratio;  // update the current ratio
    } else {
      // local to remote migration
      place_all_pages(mem_segments, (tried_ratio + ADAPTATION_STEP));
    }
  }
  LINFOF("Direction: %d, current(BE): %.10lf, tried(BE): %.10lf, diff: %.10lf",
         direction, stall_rate.at(BE), rl_str.at(BE), tried_diff);

  return direction;
}

/*
 * MBA only Mode
 *
 */
void mba_only() {
  while (run) {
    // TODO: this can be inside the loop or outside the loop!
    // TODO: Define the operation region of the controller
    LINFO("======================================================");
    LINFOF("Starting a new iteration: %d", iter);
    LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_latest_percentile_latency();

    // update the BE best stall rate
    best_stall_rate.at(BE) = std::min(best_stall_rate.at(BE),
                                      stall_rate.at(BE));

    // log the measurements for the debugging purposes!
    std::string my_action = "iteration-" + std::to_string(iter);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    if (current_latency != 0 && current_latency > target_slo * (1 + delta_hp)) {
      LINFOF(
          "SLO has been violated (ABOVE operation region) target: %.0lf, " "current: %.0lf",
          target_slo, current_latency);

      if (current_remote_ratio != 0) {
        // Enforce MBA
        LINFO("------------------------------------------------------");
        optimal_mba = search_optimal_mba();
      } else {
        LINFO("Nothing can be done about SLO violation (Change in workload!), "
              "Find new target SLO!");
        LINFOF("target: %.0lf, current: %.0lf", target_slo, current_latency);
      }
    }

    /*  else {
     LINFOF(
     "SLO has NOT been violated (BELOW operation region) target: %.0lf, "
     "current: %.0lf",
     target_slo, current_latency);

     // first release mba if any
     // Release MBA
     while (optimal_mba != 100 && !optimization_complete) {
     LINFO("------------------------------------------------------");
     optimal_mba = release_mba();
     }

     /*
     * After a new iteraion check if the current stall rate is within the
     * optimal region
     */
    /*    double diff = stall_rate.at(BE) - best_stall_rate.at(BE);
     LINFOF("BE current: %.10lf, BE best: %.10lf, diff: %.10lf",
     stall_rate.at(BE), best_stall_rate.at(BE), diff);

     if (abs(diff) > phase_change &&
     abs(diff) != std::numeric_limits<double>::infinity()) {
     optimization_complete = false;
     LINFOF("Phase change detected, diff: %.10lf", diff);
     // reset the best ratio value!
     best_stall_rate.at(BE) = std::numeric_limits<double>::infinity();
     // fix this
     // check the direction of the optimization process!
     int direction = check_opt_direction();

     if (direction == 1) {
     current_remote_ratio = apply_pagemigration_rl_be();
     } else {
     current_remote_ratio = apply_pagemigration_lr();
     }
     } else {
     if (!optimization_complete) {
     if ((diff < -(delta_be)) ||
     diff == -(std::numeric_limits<double>::infinity()) || diff == 0)
     { current_remote_ratio = apply_pagemigration_lr(); } else if ((diff >
     delta_be) || diff == -(std::numeric_limits<double>::infinity())) {
     current_remote_ratio = apply_pagemigration_rl_be();
     } else if ((diff > -(delta_be) && diff < delta_be) ||
     diff == -(std::numeric_limits<double>::infinity())) {
     LINFOF(
     "Nothing can be done (SLO within the operation region && No "
     "performance improvement for BE), delta_be: %.10lf",
     diff);
     // update the BE best stall rate
     best_stall_rate.at(BE) =
     std::min(best_stall_rate.at(BE), stall_rate.at(BE));
     } else {
     LINFO("Something else happened");
     exit(EXIT_FAILURE);
     }
     }
     }
     }

     LINFO("OPTIMIZATION COMPLETED!");
     optimization_complete = true;*/

    LINFOF("End of iteration: %d, sleeping for %d seconds", iter, sleeptime);
    LINFOF("current_remote_ratio: %d, optimal_mba: %d", current_remote_ratio,
           optimal_mba);
    iter++;

    // print_logs();
    sleep(sleeptime);
  }
}

/*
 * Apply the minimal MBA immediately
 * minimum MBA = 10
 */
void mba_10() {
  LINFOF("Monitoring period: %d ms", sleeptime);
  while (run) {
    // TODO: this can be inside the loop or outside the loop!
    // TODO: Define the operation region of the controller
    // LINFO("======================================================");
    // LINFOF("Starting a new iteration: %d", iter);
    // LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    //  stall_rate =
    //      get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_latest_percentile_latency();
    slack = (target_slo - current_latency) / target_slo;

    // for xapian, TODO: Make this dynamic
    current_latency_xpn = get_latest_percentile_latency_xpn();
    slack_xpn = (target_slo_xapian - current_latency_xpn) / target_slo_xapian;

    // update the BE best stall rate
    //  best_stall_rate.at(BE) =
    //     std::min(best_stall_rate.at(BE), stall_rate.at(BE));

    // log the measurements for the debugging purposes!
    std::string my_action = "iteration-" + std::to_string(iter);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    if (slack < slack_up || slack_xpn < slack_up) {
      // if (current_latency != 0 && current_latency > target_slo * (1 +
      // delta_hp)) {
      LINFOF(
          "SLO has been violated (ABOVE operation region) slack: %.2lf, " "target: %.0lf, " "current: %.0lf",
          slack, target_slo, current_latency);

      if (current_remote_ratio != 0 && optimal_mba != 10) {
        // Enforce MBA of 10
        LINFO("------------------------------------------------------");
        apply_mba(10);
        sleep(3);
        optimal_mba = 10;
      } else {
        LINFO("Nothing can be done about SLO violation (Change in workload!), "
              "Find new target SLO!");
        LINFOF("target: %.0lf, current: %.0lf", target_slo, current_latency);
      }
    }

    /*  else {
     LINFOF(
     "SLO has NOT been violated (BELOW operation region) target: %.0lf, "
     "current: %.0lf",
     target_slo, current_latency);

     // first release mba if any
     // Release MBA
     while (optimal_mba != 100 && !optimization_complete) {
     LINFO("------------------------------------------------------");
     optimal_mba = release_mba();
     }

     /*
     * After a new iteraion check if the current stall rate is within the
     * optimal region
     */
    /*    double diff = stall_rate.at(BE) - best_stall_rate.at(BE);
     LINFOF("BE current: %.10lf, BE best: %.10lf, diff: %.10lf",
     stall_rate.at(BE), best_stall_rate.at(BE), diff);

     if (abs(diff) > phase_change &&
     abs(diff) != std::numeric_limits<double>::infinity()) {
     optimization_complete = false;
     LINFOF("Phase change detected, diff: %.10lf", diff);
     // reset the best ratio value!
     best_stall_rate.at(BE) = std::numeric_limits<double>::infinity();
     // fix this
     // check the direction of the optimization process!
     int direction = check_opt_direction();

     if (direction == 1) {
     current_remote_ratio = apply_pagemigration_rl_be();
     } else {
     current_remote_ratio = apply_pagemigration_lr();
     }
     } else {
     if (!optimization_complete) {
     if ((diff < -(delta_be)) ||
     diff == -(std::numeric_limits<double>::infinity()) || diff == 0)
     { current_remote_ratio = apply_pagemigration_lr(); } else if ((diff >
     delta_be) || diff == -(std::numeric_limits<double>::infinity())) {
     current_remote_ratio = apply_pagemigration_rl_be();
     } else if ((diff > -(delta_be) && diff < delta_be) ||
     diff == -(std::numeric_limits<double>::infinity())) {
     LINFOF(
     "Nothing can be done (SLO within the operation region && No "
     "performance improvement for BE), delta_be: %.10lf",
     diff);
     // update the BE best stall rate
     best_stall_rate.at(BE) =
     std::min(best_stall_rate.at(BE), stall_rate.at(BE));
     } else {
     LINFO("Something else happened");
     exit(EXIT_FAILURE);
     }
     }
     }
     }

     LINFO("OPTIMIZATION COMPLETED!");
     optimization_complete = true;*/

    /* LINFOF("End of iteration: %d, sleeping for %d seconds", iter, sleeptime);
     LINFOF(
     "current_remote_ratio: %d, optimal_mba: %d, current_latency: %.0lf, "
     "slack: %.2lf",
     current_remote_ratio, optimal_mba, current_latency, slack);*/
    iter++;

    // print_logs();
    usleep(sleeptime);
  }
}

/*
 * Disable the controller only allowing the page migration
 *
 */
void disabled_controller() {
  while (run) {
    // TODO: this can be inside the loop or outside the loop!
    // TODO: Define the operation region of the controller
    LINFO("======================================================");
    LINFOF("Starting a new iteration: %d", iter);
    LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_percentile_latency();

    // update the BE best stall rate
    best_stall_rate.at(BE) = std::min(best_stall_rate.at(BE),
                                      stall_rate.at(BE));

    // log the measurements for the debugging purposes!
    std::string my_action = "iteration-" + std::to_string(iter);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    LINFOF(
        "Optimizing page migration without considering SLO target: %.0lf, " "current: %.0lf",
        target_slo, current_latency);

    /*
     * After a new iteraion check if the current stall rate is within the
     * optimal region
     */
    double diff = stall_rate.at(BE) - best_stall_rate.at(BE);
    LINFOF("BE current: %.10lf, BE best: %.10lf, diff: %.10lf",
           stall_rate.at(BE), best_stall_rate.at(BE), diff);

    if (abs(diff) > phase_change
        && abs(diff) != std::numeric_limits<double>::infinity()) {
      optimization_complete = false;
      LINFOF("Phase change detected, diff: %.10lf", diff);
      // reset the best ratio value!
      best_stall_rate.at(BE) = std::numeric_limits<double>::infinity();
      // fix this
      // check the direction of the optimization process!
      int direction = check_opt_direction();

      if (direction == 1) {
        current_remote_ratio = apply_pagemigration_rl_be();
      } else {
        current_remote_ratio = apply_pagemigration_lr_dc();
      }
    } else {
      if (!optimization_complete) {
        if ((diff < -(delta_be))
            || diff == -(std::numeric_limits<double>::infinity())
            || diff == 0) {
          current_remote_ratio = apply_pagemigration_lr_dc();
        } else if ((diff > delta_be)
            || diff == -(std::numeric_limits<double>::infinity())) {
          current_remote_ratio = apply_pagemigration_rl_be();
        } else if ((diff > -(delta_be) && diff < delta_be)
            || diff == -(std::numeric_limits<double>::infinity())) {
          LINFOF(
              "Nothing can be done (SLO within the operation region && No " "performance improvement for BE), delta_be: %.10lf",
              diff);
          // update the BE best stall rate
          best_stall_rate.at(BE) = std::min(best_stall_rate.at(BE),
                                            stall_rate.at(BE));
        } else {
          LINFO("Something else happened");
          exit(EXIT_FAILURE);
        }
      }
    }

    LINFO("OPTIMIZATION COMPLETED!");
    optimization_complete = true;

    LINFOF("End of iteration: %d, sleeping for %d seconds", iter, sleeptime);
    LINFOF("current_remote_ratio: %d, optimal_mba: %d", current_remote_ratio,
           optimal_mba);
    iter++;

    // print_logs();
    sleep(sleeptime);
  }
}

/*
 * linux default Mode
 *
 */
void linux_default() {
  LINFOF("Monitoring period: %d ms", sleeptime);
  while (run) {
    // LINFO("======================================================");
    // LINFOF("Starting a new iteration: %d", iter);
    // LINFO("------------------------------------------------------");

    // Measure the stall_rate of the applications
    // stall_rate =
    //     get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the 99th percentile of the HP application
    current_latency = get_latest_percentile_latency();
    slack = (target_slo - current_latency) / target_slo;

    // for xapian, TODO: Make this dynamic
    current_latency_xpn = get_latest_percentile_latency_xpn();
    slack_xpn = (target_slo_xapian - current_latency_xpn) / target_slo_xapian;

    /*  LINFOF(
     "target(HP): %.0lf, current(HP): %.0lf, BE current: %.10lf, HP "
     "current: %.10lf",
     target_slo, current_latency, stall_rate.at(BE), stall_rate.at(HP));*/

    std::string my_action = "iter-" + std::to_string(iter);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    iter++;

    // print_logs();
    usleep(sleeptime);
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
  stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                      _num_poll_outliers);

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
/*double get_percentile_latency() {
 double service_time;

 // LINFO("Getting the current percentile latency for the HP");

 // poll the latency of the applications
 service_time = connect_to_client();

 return service_time;
 }*/

double get_percentile_latency() {
  double service_time = 0;

  try {
    // socket creation
    boost::system::error_code error;
    boost::asio::io_service io_service;
    tcp::socket socket(io_service);

    // connection
    // std::cout << "[Client] Connecting to server..." << std::endl;
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

      std::string my_string(buf.begin(), len);
      // std::copy(buf.begin(), buf.begin()+len, std::back_inserter(my_string));
      // std::cout.write(buf.data(), len);
      // cout << std::endl;
      // cout << my_string << std::endl;
      service_time = boost::lexical_cast<double>(my_string);
    }
  } catch (std::exception &e) {
    //LINFO("Problem connecting to the client");
    //std::cerr << e.what() << std::endl;
    // exit(EXIT_FAILURE);
    // Don't terminate just return 0
    // terminateHandler();
  }

  return service_time;
}

double get_percentile_latency_xpn() {
  double service_time = 0;
  // for now this is hard-coded
  int port2 = 1235;
  //server = "146.193.41.140";

  try {
    // socket creation
    boost::system::error_code error;
    boost::asio::io_service io_service;
    tcp::socket socket(io_service);

    // connection
    // std::cout << "[Client] Connecting to server..." << std::endl;
    socket.connect(
        tcp::endpoint(boost::asio::ip::address::from_string(server), port2),
        error);

    for (;;) {
      boost::array<char, 128> buf;

      size_t len = socket.read_some(boost::asio::buffer(buf), error);

      if (error == boost::asio::error::eof)
        break;  // Connection closed cleanly by peer.
      else if (error)
        throw boost::system::system_error(error);  // Some other error.

      std::string my_string(buf.begin(), len);
      // std::copy(buf.begin(), buf.begin()+len, std::back_inserter(my_string));
      // std::cout.write(buf.data(), len);
      // cout << std::endl;
      // cout << my_string << std::endl;
      service_time = boost::lexical_cast<double>(my_string);
      service_time = std::ceil(service_time / 1e6 * 100) / 100;
    }
  } catch (std::exception &e) {
    //LINFO("Problem connecting to the client");
    //std::cerr << e.what() << std::endl;
    // exit(EXIT_FAILURE);
    //terminateHandler();
  }

  return service_time;
}

/*
 start the measurement thread!
 */
void spawn_measurement_thread() {
  std::cout << "Starting measurement thread.\n";
  std::thread t(measurement_collector);
  // do not wait it to finish
  t.detach();
}

void measurement_collector() {
  double cpl = 0;
  double cpl_xpn = 0;
  while (run) {
    cpl = get_percentile_latency();
    cpl_xpn = get_percentile_latency_xpn();
    percentile_samples.push_back(cpl);
    percentile_samples_xpn.push_back(cpl_xpn);
    // TODO: factor this out!
    if (((target_slo - cpl) / target_slo) <= slack_up) {
      violations_counter_f++;
    }
    if (cpl > target_slo) {
      violations_counter_t++;
    }

    if (((target_slo_xapian - cpl_xpn) / target_slo_xapian) <= slack_up) {
      vlts_cnt_f++;
    }
    if (cpl_xpn > target_slo_xapian) {
      vlts_cnt_t++;
    }

    usleep(20000);
  }
}

double get_latest_percentile_latency() {
  if (!percentile_samples.empty()) {
    return percentile_samples.back();
  } else {
    return 0;
  }
}

double get_latest_percentile_latency_xpn() {
  if (!percentile_samples_xpn.empty()) {
    return percentile_samples_xpn.back();
  } else {
    return 0;
  }
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
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    // Measure the current latency
    current_latency = get_latest_percentile_latency();

    std::string my_action = "apply_mba-" + std::to_string(i);
    my_logger(chrono::system_clock::now(), current_remote_ratio, i, target_slo,
              current_latency, slack, stall_rate.at(HP), stall_rate.at(BE),
              my_action, logCounter++);

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
int apply_pagemigration_rl() {
  int i;
  // apply the next ratio immediately
  if (current_remote_ratio > 0) {
    current_remote_ratio -= ADAPTATION_STEP;
  }

  for (i = current_remote_ratio; i >= 0; i -= ADAPTATION_STEP) {
    // LINFOF("Going to check a ratio of %d", i);
    place_all_pages(mem_segments, i);
    // break;
    // Measure the stall_rate of the applications
    //  stall_rate =
    //      get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // sleep for 100ms
    // usleep(100000);
    // sleep for 1 sec
    sleep(3);
    // usleep(sleeptime);
    // Measure the current latency measurement
    current_latency = get_latest_percentile_latency();
    slack = (target_slo - current_latency) / target_slo;
    // update the BE best stall rate
    // best_stall_rate.at(BE) =
    //    std::min(best_stall_rate.at(BE), stall_rate.at(BE));

    std::string my_action = "apply_ratio-" + std::to_string(i);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    // sanity check
    /*  if (current_latency == 0) {
     LINFOF(
     "NAN HP latency (STOP page migration): target: %.0lf, current:
     %.0lf", target_slo, current_latency); current_remote_ratio = i; break;
     }*/

    // check if to use the slack_up or slack_down functions!
    // if (slack > slack_down_pg) {
    if (slack > slack_up) {
      // if (current_latency <= target_slo * (1 + delta_hp)) {
      LINFOF(
          "SLO has been achieved (STOP page migration): target: %.0lf, " "current: %.0lf",
          target_slo, current_latency);
      current_remote_ratio = i;
      break;
    } else {
      LINFOF(
          "SLO has NOT been achieved (CONTINUE page migration): target: %.0lf, " "current: %.0lf",
          target_slo, current_latency);
      current_remote_ratio = i;
    }
  }

  LINFOF(
      "Current remote ratio: %d, Optimal mba: %d, latency: %.0lf, slack: %.2lf",
      current_remote_ratio, optimal_mba, current_latency, slack);
  return current_remote_ratio;
}

/*
 * Fix SLO violations by moving pages from local to remote node
 * This function assumes that SLO violations are due to memory bandwidth
 * contention in the same socket
 */

int apply_pagemigration_lr_same_socket() {
  int i;
  // apply the next ratio immediately
  if (current_remote_ratio == 100) {
    return current_remote_ratio;
  }
  if (current_remote_ratio < 100) {
    current_remote_ratio += ADAPTATION_STEP;
  }

  for (i = current_remote_ratio; i <= 100; i += ADAPTATION_STEP) {
    // LINFOF("Going to check a ratio of %d", i);
    place_all_pages(mem_segments, i);
    // break;
    // Measure the stall_rate of the applications
    //  stall_rate =
    //      get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // sleep for 100ms
    // usleep(100000);
    // sleep for 1 sec
    sleep(3);
    // usleep(sleeptime);
    // Measure the current latency measurement
    current_latency = get_latest_percentile_latency();
    slack = (target_slo - current_latency) / target_slo;

    // for xapian, TODO: Make this dynamic
    current_latency_xpn = get_latest_percentile_latency_xpn();
    slack_xpn = (target_slo_xapian - current_latency_xpn) / target_slo_xapian;
    // update the BE best stall rate
    // best_stall_rate.at(BE) =
    //    std::min(best_stall_rate.at(BE), stall_rate.at(BE));

    std::string my_action = "apply_ratio-" + std::to_string(i);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    // sanity check
    /*  if (current_latency == 0) {
     LINFOF(
     "NAN HP latency (STOP page migration): target: %.0lf, current:
     %.0lf", target_slo, current_latency); current_remote_ratio = i; break;
     }*/

    // check if to use the slack_up or slack_down functions!
    // if (slack > slack_down_pg) {
    if (slack > slack_up || slack_xpn > slack_up) {
      // if (current_latency <= target_slo * (1 + delta_hp)) {
      LINFOF(
          "SLO has been achieved (STOP page migration): target: %.0lf, " "current: %.0lf",
          target_slo, current_latency);
      current_remote_ratio = i;
      break;
    } else {
      LINFOF(
          "SLO has NOT been achieved (CONTINUE page migration): target: %.0lf, " "current: %.0lf",
          target_slo, current_latency);
      current_remote_ratio = i;
    }
  }

  LINFOF(
      "Current remote ratio: %d, Optimal mba: %d, latency: %.0lf, slack: %.2lf",
      current_remote_ratio, optimal_mba, current_latency, slack);
  return current_remote_ratio;
}

/*
 * Page migrations from remote to local node (HP to BE nodes)
 * considering only the optimization of BE
 * TODO: Handle transient cases
 */
int apply_pagemigration_rl_be() {
  int i;
  // apply the next ratio immediately
  if (current_remote_ratio > 0) {
    current_remote_ratio -= ADAPTATION_STEP;
  }

  for (i = current_remote_ratio; i >= 0; i -= ADAPTATION_STEP) {
    LINFOF("Going to check a ratio of %d", i);
    place_all_pages(mem_segments, i);

    // Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    // Measure the current latency measurement
    current_latency = get_percentile_latency();

    // update the BE best stall rate
    best_stall_rate.at(BE) = std::min(best_stall_rate.at(BE),
                                      stall_rate.at(BE));

    std::string my_action = "apply_ratio-" + std::to_string(i);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    // sanity check
    /*   if (current_latency == 0) {
     LINFOF(
     "NAN HP latency (STOP page migration): target: %.0lf, current:
     %.0lf", target_slo, current_latency); current_remote_ratio = i; break;
     }*/
    double diff = stall_rate.at(BE) - best_stall_rate.at(BE);

    if (diff > delta_be) {
      LINFOF(
          "page optimization achieved (STOP page migration): target: %.0lf, " "current: %.0lf, delta: %.10lf",
          target_slo, current_latency, diff);
      LINFOF("current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf",
             stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE));
      if (i != 100) {
        LINFO("Going one step back before breaking!");
        place_all_pages(mem_segments, (i + ADAPTATION_STEP));
        current_remote_ratio = i + ADAPTATION_STEP;
      } else {
        current_remote_ratio = i;
      }
      // current_remote_ratio = i;
      break;
    } else {
      LINFOF(
          "page optimazation possible (CONTINUE page migration): target: " "%.0lf, " "current: %.0lf",
          target_slo, current_latency);
      LINFOF("current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf",
             stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE));
      current_remote_ratio = i;
    }
  }

  LINFOF("Current remote ratio: %d", current_remote_ratio);
  return current_remote_ratio;
}

/*
 * Page migrations from local to remote node (BE to HP nodes)
 * to optimize the BW of BE
 * TODO: Handle transient cases - fixed
 *
 */

int apply_pagemigration_lr() {
  int i;
  // apply the next ratio immediately
  if (current_remote_ratio < 100) {
    current_remote_ratio += ADAPTATION_STEP;
  }

  // for (i = 60; i <= 100; i += 10){
  for (i = current_remote_ratio; i <= 100; i += ADAPTATION_STEP) {
    LINFOF("Going to check a ratio of %d", i);
    if (i != 0) {
      place_all_pages(mem_segments, i);
    }

    // Measure the stall_rate of the applications
    // stall_rate =
    //     get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // sleep(sleeptime);
    sleep(3);
    // Measure the current latency measurement
    /*current_latency = get_percentile_latency();
     // First check if we are violating the SLO
     slack = (target_slo - current_latency) / target_slo;

     // update the BE best stall rate
     // best_stall_rate.at(BE) =
     //     std::min(best_stall_rate.at(BE), stall_rate.at(BE));

     // current diff
     // double my_diff = stall_rate.at(BE) - best_stall_rate.at(BE);

     std::string my_action = "apply_ratio-" + std::to_string(i);
     my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
     target_slo, current_latency, slack, stall_rate.at(HP),
     stall_rate.at(BE), my_action, logCounter++);

     if (slack < slack_up) {
     // if (current_latency != 0 && current_latency > target_slo * (1 +
     // delta_hp)) {
     LINFOF(
     "SLO has been violated target: %.0lf, current(HP): %.0lf, slack: "
     "%.2lf",
     target_slo, current_latency, slack);
     // LINFOF("current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf",
     //        stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE));
     if (i != 0) {
     LINFO("Going one step back before breaking!");
     place_all_pages(mem_segments, (i - ADAPTATION_STEP));
     current_remote_ratio = i - ADAPTATION_STEP;
     } else {
     current_remote_ratio = i;
     }
     break;
     } else if (slack > slack_down_pg) {
     LINFOF(
     "Below the green region, continue climbing, current: %.0lf, slack: "
     "%.2lf",
     current_latency, slack);
     current_remote_ratio = i;
     } else {
     LINFOF(
     "Within the green region, stop climbing, current: %.0lf, slack: "
     "%.2lf",
     current_latency, slack);
     current_remote_ratio = i;
     break;
     }*/

    // then check if there is any performance improvement for BE
    /* else if (my_diff > delta_be || std::isnan(stall_rate.at(BE))) {
     LINFOF(
     "No performance improvement for the BE, target: %.0lf, current(HP): "
     "%.0lf",
     target_slo, current_latency);
     LINFOF(
     "current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf, diff: "
     "%.10lf",
     stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE),
     my_diff);
     // just make sure that its not something transient...!
     LINFO("Hmm... Is this the best we can do?");
     std::vector<double> stall_rate_transient = get_average_stall_rate(
     _num_polls * 2, _poll_sleep, _num_poll_outliers * 2);
     if ((stall_rate_transient.at(BE) - best_stall_rate.at(BE)) > delta_be ||
     std::isnan(stall_rate_transient.at(BE))) {
     LINFOF("I guess so!, transient(BE): %.10lf",
     stall_rate_transient.at(BE));
     if (i != 0) {
     LINFO("Going one step back before breaking!");
     place_all_pages(mem_segments, (i - ADAPTATION_STEP));
     current_remote_ratio = i - ADAPTATION_STEP;
     } else {
     current_remote_ratio = i;
     }
     break;
     }
     }

     else if (my_diff != 0 && my_diff > -(delta_be) && my_diff < delta_be) {
     LINFOF(
     "No performance improvement for the BE, in the operation region!, "
     "target: %.0lf, current(HP): %.0lf",
     target_slo, current_latency);
     LINFOF(
     " current(HP): % .10lf, best(BE): % .10lf, current(BE): % .10lf, "
     "diff: %.10lf",
     stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE),
     my_diff);
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
     "latency(HP): %.0lf, diff: %.10lf",
     stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE),
     current_latency, my_diff);
     current_remote_ratio = i;
     }
     */
  }

  LINFOF("Current remote ratio: %d", current_remote_ratio);
  return current_remote_ratio;
}

/*
 * Page migrations from local to remote node (BE to HP nodes)
 * to optimize the BW of BE
 * this function does not care about slo violations!!
 * TODO: Handle transient cases - fixed
 *
 */

int apply_pagemigration_lr_dc() {
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
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    // Measure the current latency measurement
    current_latency = get_percentile_latency();

    // update the BE best stall rate
    best_stall_rate.at(BE) = std::min(best_stall_rate.at(BE),
                                      stall_rate.at(BE));

    // current diff
    double my_diff = stall_rate.at(BE) - best_stall_rate.at(BE);

    std::string my_action = "apply_ratio-" + std::to_string(i);
    my_logger(chrono::system_clock::now(), current_remote_ratio, optimal_mba,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    /// Do not care about slo violation just optimize page migration!
    // Check if there is any performance improvement for BE
    if (my_diff > delta_be || std::isnan(stall_rate.at(BE))) {
      LINFOF(
          "No performance improvement for the BE, target: %.0lf, current(HP): " "%.0lf",
          target_slo, current_latency);
      LINFOF(
          "current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf, diff: " "%.10lf",
          stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE), my_diff);
      // just make sure that its not something transient...!
      LINFO("Hmm... Is this the best we can do?");
      std::vector<double> stall_rate_transient = get_average_stall_rate(
          _num_polls * 2, _poll_sleep, _num_poll_outliers * 2);
      if ((stall_rate_transient.at(BE) - best_stall_rate.at(BE)) > delta_be
          || std::isnan(stall_rate_transient.at(BE))) {
        LINFOF("I guess so!, transient(BE): %.10lf",
               stall_rate_transient.at(BE));
        if (i != 0) {
          LINFO("Going one step back before breaking!");
          place_all_pages(mem_segments, (i - ADAPTATION_STEP));
          current_remote_ratio = i - ADAPTATION_STEP;
        } else {
          current_remote_ratio = i;
        }
        break;
      }
    }

    else if (my_diff != 0 && my_diff > -(delta_be) && my_diff < delta_be) {
      LINFOF(
          "No performance improvement for the BE, in the operation region!, " "target: %.0lf, current(HP): %.0lf",
          target_slo, current_latency);
      LINFOF(
          " current(HP): % .10lf, best(BE): % .10lf, current(BE): % .10lf, " "diff: %.10lf",
          stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE), my_diff);
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
      LINFO("Performance improvement for BE without SLO Violation, continue "
            "climbing");
      LINFOF(
          "current(HP): %.10lf, best(BE): %.10lf, current(BE): %.10lf, " "latency(HP): %.0lf, diff: %.10lf",
          stall_rate.at(HP), best_stall_rate.at(BE), stall_rate.at(BE),
          current_latency, my_diff);
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

  // if the current ratio is zero, then apply the max mba immediately incase of
  // multi-socket colocation or if the current ratio is 100, them apply the max
  // mba immediately incase of single-socket colocation
  if (current_remote_ratio == 0) {
    apply_mba(100);

    // sleep for 100ms
    // usleep(100000);
    // sleep for 1 sec
    sleep(3);
    // usleep(sleeptime);
    // Measure the stall_rate of the applications
    // stall_rate =
    //     get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the current latency
    current_latency = get_latest_percentile_latency();
    slack = (target_slo - current_latency) / target_slo;

    current_latency_xpn = get_latest_percentile_latency_xpn();
    slack_xpn = (target_slo_xapian - current_latency_xpn) / target_slo_xapian;

    std::string my_action = "apply_mba-" + std::to_string(100);
    my_logger(chrono::system_clock::now(), current_remote_ratio, 100,
              target_slo, current_latency, slack, stall_rate.at(HP),
              stall_rate.at(BE), my_action, logCounter++);

    optimal_mba = 100;

    // LINFOF("Optimal MBA: %d", optimal_mba);
    LINFOF(
        "Current remote ratio: %d, Optimal mba: %d, latency: %.0lf, slack: " "%.2lf",
        current_remote_ratio, optimal_mba, current_latency, slack);
    return optimal_mba;
  }

  for (i = optimal_mba; i <= 100; i += 10) {
    if (i == 70 || i == 80)
      continue;

    apply_mba(i);

    // sleep for 100ms
    // usleep(100000);
    sleep(3);
    // sleep for 1 sec
    // usleep(sleeptime);
    // Measure the stall_rate of the applications
    // stall_rate =
    //    get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

    // Measure the current latency
    current_latency = get_latest_percentile_latency();
    slack = (target_slo - current_latency) / target_slo;

    current_latency_xpn = get_latest_percentile_latency_xpn();
    slack_xpn = (target_slo_xapian - current_latency_xpn) / target_slo_xapian;

    std::string my_action = "apply_mba-" + std::to_string(i);
    my_logger(chrono::system_clock::now(), current_remote_ratio, i, target_slo,
              current_latency, slack, stall_rate.at(HP), stall_rate.at(BE),
              my_action, logCounter++);

    // only release mba while we are in the green zone!
    if (slack > slack_down_mba && slack_xpn > slack_down_mba
        && current_remote_ratio != 0) {
      //  if (current_latency != 0 && current_latency > target_slo * (1 +
      //  delta_hp) &&
      //     current_remote_ratio != 0) {
      LINFOF(
          "SLO violation has NOT been detected (CONTINUE releasing MBA): " "target: %.0lf, current: %.0lf, slack: %.2lf",
          target_slo, current_latency, slack);
      optimal_mba = i;
    } else {
      LINFOF(
          "SLO violation has been detected (STOP releasing MBA and " "revert-back): target: " "%.0lf, current: %.0lf, slack: %.2lf",
          target_slo, current_latency, slack);
      // revert_back to the previous mba
      apply_mba(i - 10);
      optimal_mba = i - 10;
      break;
    }
  }

  // LINFOF("Optimal MBA: %d", optimal_mba);
  LINFOF(
      "Current remote ratio: %d, Optimal mba: %d, latency: %.0lf, slack: %.2lf",
      current_remote_ratio, optimal_mba, current_latency, slack);

  return optimal_mba;
}

/*
 * Apply a single MBA value
 * TODO: Avoid using a system call to do this, instead use the library directly!
 */
void apply_mba(int mba_value) {
  LINFOF("Applying MBA of %d", mba_value);
  /*char buf[32];
   sprintf(buf, "sudo pqos -e 'mba@0:0=%d'", mba_value);
   system(buf); */
  // use cos 1 and socket 0
  // TODO: specify this as parameters
  set_mba_parameters(1, mba_value);
  int ret = set_mba_allocation(0);
  if (ret < 0) {
    LINFO("Allocation configuration error!");
    exit(EXIT_FAILURE);
  }
  LINFO("Allocation configuration altered.");
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
  /*LINFO("==============================================");
   LINFO("TESTING get_target_stall_rate function");
   LINFO("----------------------------------------------");
   LINFOF("Target SLO at this point: %.0lf", target_slo);

   // Measure the stall_rate of the applications
   stall_rate =
   get_average_stall_rate(_num_polls, _poll_sleep, _num_poll_outliers);

   // Measure the latency measurement
   current_latency = get_latest_percentile_latency();

   LINFOF("Stall rate: target: %.0lf, current: %.10lf, latency: %.0lf",
   target_slo, stall_rate.at(HP), current_latency);

   LINFO("==============================================");
   LINFO("TESTING search_optimal_mba function");
   LINFO("----------------------------------------------");
   optimal_mba = search_optimal_mba();

   */
  sleep(10);
  LINFO("==============================================");
  LINFO("TESTING apply_pagemigration_lr function");
  LINFO("----------------------------------------------");
  current_remote_ratio = apply_pagemigration_lr();

  /*LINFO("==============================================");
   LINFO("TESTING apply_pagemigration_rl function");
   LINFO("----------------------------------------------");
   current_remote_ratio = apply_pagemigration_rl();*/

  /*LINFO("==============================================");
   LINFO("TESTING apply_pagemigration_lr_same_socket function");
   LINFO("----------------------------------------------");
   current_remote_ratio = apply_pagemigration_lr_same_socket();*/

  /*LINFO("==============================================");
   LINFO("TESTING release_mba function");
   LINFO("----------------------------------------------");
   optimal_mba = release_mba();*/

  LINFO("==============================================");
  LINFOF("FINAL VALUES: current_optimal_mba: %d, current_remote_ratio: %d",
         optimal_mba, current_remote_ratio);
}

void find_optimal_lr_ratio() {
  LINFO("==============================================");
  LINFO("Finding optimal Local-remote ratio for BE");
  LINFO("----------------------------------------------");
  current_remote_ratio = apply_pagemigration_lr();

  // print the logs
  print_logs();
}

/*
 * print the logs
 *
 */
void print_logs() {
  for (size_t j = 0; j < my_logs.size(); j++) {
    std::cout << std::fixed;
    std::cout << std::setprecision(10);

    std::time_t now_c = std::chrono::system_clock::to_time_t(
        my_logs.at(j).timenow);

    cout << now_c << "\t" << my_logs.at(j).current_remote_ratio << "\t"
         << my_logs.at(j).current_mba_level << "\t"
         << (int) my_logs.at(j).HPA_target_slo << "\t"
         << (int) my_logs.at(j).HPA_currency_latency << "\t"
         << my_logs.at(j).HPA_slack << "\t" << my_logs.at(j).HPA_stall_rate
         << "\t" << my_logs.at(j).BEA_stall_rate << "\t" << my_logs.at(j).action
         << "\t" << my_logs.at(j).logCounter << endl;
  }
}

void print_logs_v2() {
  /*cout << "time (sec)"
   << "\t"
   << "Counter"
   << "\t"
   << "QoS" << endl;

   std::time_t start_c =
   std::chrono::system_clock::to_time_t(my_logs.at(0).timenow);

   for (size_t j = 0; j < my_logs.size(); j++) {
   std::cout << std::fixed;
   std::cout << std::setprecision(10);

   std::time_t now_c =
   std::chrono::system_clock::to_time_t(my_logs.at(j).timenow);

   cout << (now_c - start_c) << "\t" << my_logs.at(j).logCounter << "\t"
   << (int)my_logs.at(j).HPA_currency_latency << endl;
   }*/
  cout << "Total violations_f:\t" << violations_counter_f << endl;
  cout << "Total violations_t:\t" << violations_counter_t << endl;
  cout << "optimal mba:\t" << optimal_mba << "\toptimal ratio:\t"
       << current_remote_ratio << endl;

  // print the violations for xapian, TODO: Make this dynamic
  cout << "xapian, Total violations_f:\t" << vlts_cnt_f << endl;
  cout << "xapian, Total violations_t:\t" << vlts_cnt_t << endl;

  // print also to a file, use append
  FILE *f = fopen("abc_numa_results_log.txt", "a");
  fprintf(f, "v_f:\t%d\tv_t:\t%d\tmba:\t%d\tlrr:\t%d\n", violations_counter_f,
          violations_counter_t, optimal_mba, current_remote_ratio);

  // print the xapian data also to a file
  fprintf(f, "xpn_v_f:\t%d\txpn_v_t:\t%d\n", vlts_cnt_f, vlts_cnt_t);
  // close the file
  fclose(f);
}

void print_to_file() {
  // log memcached latency data into a file
  FILE *f = fopen("memcached_latency_log.txt", "w");
  for (size_t j = 0; j < percentile_samples.size(); j++) {
    // fprintf(f, "%d\n", (int)my_logs.at(j).HPA_currency_latency);
    fprintf(f, "%d\t%d\n", (int) j, (int) percentile_samples.at(j));
  }
  /* close the file*/
  fclose(f);

  // log xapian latency data also into a file: TODO: factor this function out!
  FILE *f_2 = fopen("xapian_latency_log.txt", "w");
  for (size_t j = 0; j < percentile_samples_xpn.size(); j++) {
    // fprintf(f, "%d\n", (int)my_logs.at(j).HPA_currency_latency);
    fprintf(f_2, "%d\t%.2f\n", (int) j, percentile_samples_xpn.at(j));
  }
  /* close the file*/
  fclose(f_2);
}

/*
 * Log all the current information
 */
void my_logger(std::chrono::system_clock::time_point tn, int crr, int cml,
               double hpt, double hcl, double slk, double hps, double bes,
               std::string action, int lc) {
  // Log all the current information
  MyLogger mylogger(tn, crr, cml, hpt, hcl, slk, hps, bes, action, lc);

  my_logs.push_back(mylogger);
}

void test_fixed_ratio() {
  stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                      _num_poll_outliers);
  LINFOF("Before: stall_rate(BE): %.10lf, stall_rate(HP): %.10lf",
         stall_rate.at(BE), stall_rate.at(HP));
  LINFOF("Going to check a ratio of %d", fixed_ratio_value);
  place_all_pages(mem_segments, fixed_ratio_value);
  stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                      _num_poll_outliers);
  LINFOF("After: stall_rate(BE): %.10lf, stall_rate(HP): %.10lf",
         stall_rate.at(BE), stall_rate.at(HP));
}

void read_weights(std::string filename) {
  FILE *fp;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  const char s[2] = ",";
  char *token;

  int j = 0;

  double weight;
  int id;

  fp = fopen(filename.c_str(), "r");
  if (fp == NULL) {
    printf("Weights have not been provided!\n");
    exit(EXIT_FAILURE);
  }

  while ((read = getline(&line, &len, fp)) != -1) {
    char *strtok_saveptr;
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
  if (line)
    free(line);

  LINFO("weights initialized!");

  return;
}
