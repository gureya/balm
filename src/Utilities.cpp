/*
 * Utilities.cpp
 *
 *  Created on: Jan 7, 2020
 *      Author: David Daharewa Gureya
 */

#include "include/Utilities.hpp"
#include "include/Logger.hpp"
#include "include/BwManager.hpp"
#include "include/PerformanceCounters.hpp"
#include "include/MySharedMemory.hpp"
#include "include/PagePlacement.hpp"

/////////////////////////////////////////////
//provide this in a config
unsigned int _wait_start = 2;
unsigned int _num_polls = 20;
unsigned int _num_poll_outliers = 5;
useconds_t _poll_sleep = 200000;
double noise_allowed = 0.05;  // 5%
////////////////////////////////////////////

static int run = 1;
static int sleeptime = 5;

enum {
  BE,
  HP
};

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
 *
 */
void periodic_monitor() {

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

  int current_remote_ratio = 0;
  int optimal_mba = 100;
  double target_stall_rate;

  std::vector<double> stall_rate(active_cpus);
  std::vector<double> prev_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());

  while (run) {

    //TODO: this can be inside the loop or outside the loop!
    //TODO: Define the operation region of the controller

    target_stall_rate = get_target_stall_rate();
    LINFOF("Target SLO at this point: %.10lf", target_stall_rate);

    //Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    if (!std::isnan(stall_rate.at(HP))
        && stall_rate.at(HP) > target_stall_rate * 1.001) {

      LINFOF("SLO has been violated target: %.10lf, current: %.10lf",
             target_stall_rate, stall_rate.at(HP));

      if (current_remote_ratio != 0) {
        //Enforce MBA
        optimal_mba = search_optimal_mba(target_stall_rate);

        //Enforce Lazy Page migration while releasing MBA
        while (optimal_mba != 100) {
          //apply page migration
          current_remote_ratio = apply_pagemigration_rl(target_stall_rate,
                                                        current_remote_ratio);
          //release MBA
          optimal_mba = release_mba(optimal_mba, target_stall_rate,
                                    current_remote_ratio);
        }

      } else {
        LINFO(
            "Nothing can be done about SLO violation (Change in workload!), Find new target SLO!");
      }

    } else {

      LINFOF("SLO has NOT been violated target: %.10lf, current: %.10lf",
             target_stall_rate, stall_rate.at(HP));

      /*
       * Optimize page migration either way (local to remote and vice versa)!
       * if the current stall rate is smaller than the previous stall rate,
       * move pages from local to remote node
       * Otherwise the BE may have become less memory-intensive,
       * move pages from remote to local node
       *
       */
      LINFOF("BE current: %.10lf, BE previous: %.10lf", stall_rate.at(BE),
             prev_stall_rate.at(BE));

      if (stall_rate.at(BE) < prev_stall_rate.at(BE)) {
        current_remote_ratio = apply_pagemigration_lr(target_stall_rate,
                                                      current_remote_ratio);
      } else {
        current_remote_ratio = apply_pagemigration_rl(target_stall_rate,
                                                      current_remote_ratio);
      }
    }

    //At the end update previous stall rate to the current stall rate!
    //TODO: This has to be debugged - stall rate has changed during the previous iterations!
    prev_stall_rate = stall_rate;

    sleep(sleeptime);
  }

}

/*
 * Get the target SLO of the high-priority task at any particular point in time
 * Set MBA to the minimum value and measure the current stall_rate
 * After measuring set MBA to the maximum value
 *
 */
double get_target_stall_rate() {

  double target_stall_rate;
  int min_mba = 10;
  int max_mba = 100;

  LINFO("===================================================");
  LINFO("Getting the target SLO for the HP");
  apply_mba(min_mba);

  std::vector<double> stall_rate(active_cpus);
  //Measure the stall_rate of the applications
  stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                      _num_poll_outliers);

  target_stall_rate = stall_rate.at(HP);

  apply_mba(max_mba);
  LINFO("===================================================");

  return target_stall_rate;
}

/*
 * Search the highest MBA that still meets the target SLO
 * Apply binary search to reduce the search space
 * Valid MBA states in our case: 100, 90, 60, 50, 40, 30, 20, 10
 * TODO: check for transient values
 *
 */

int search_optimal_mba(double target_stall_rate) {

  int i;
  double progress;
  int optimal_mba;

  std::vector<double> stall_rate(active_cpus);

  i = 40;
  int previous_mba = 40;

  while (i != 10 || i != 30 || i != 50 || i != 90) {

    if (i == 0) {
      LINFO("End of valid MBA states, breaking!");
      optimal_mba = previous_mba;
      break;
    }

    apply_mba(i);

    //Measure the stall_rate of the applications after enforcing MBA
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    if (stall_rate.at(HP) <= target_stall_rate) {
      LINFOF("SLO has been achieved: target: %.10lf, current: %.10lf",
             target_stall_rate, stall_rate.at(1));
      optimal_mba = i;
      break;
    }

    else {
      LINFOF("SLO has NOT been achieved: target: %.10lf, current: %.10lf",
             target_stall_rate, stall_rate.at(HP));
    }

    progress = stall_rate.at(HP) - target_stall_rate;
    LINFOF("Progress: %.10lf", progress);
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
int apply_pagemigration_rl(double target_stall_rate, int current_remote_ratio) {

  std::vector<double> stall_rate(active_cpus);
  int i;

  for (i = current_remote_ratio; i > 0; i -= ADAPTATION_STEP) {

    LINFOF("Going to check a ratio of %d", i);
    //place_all_pages(mem_segments, i);

    //Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    if (stall_rate.at(HP) <= target_stall_rate) {

      LINFOF(
          "SLO has been achieved (STOP page migration): target: %.10lf, current: %.10lf",
          target_stall_rate, stall_rate.at(HP));
      current_remote_ratio = i;
      break;
    } else {
      LINFOF(
          "SLO has NOT been achieved (CONTINUE page migration): target: %.10lf, current: %.10lf",
          target_stall_rate, stall_rate.at(HP));
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

int apply_pagemigration_lr(double target_stall_rate, int current_remote_ratio) {

  std::vector<double> best_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());
  std::vector<double> stall_rate(active_cpus);
  int i;

  for (i = current_remote_ratio; i <= 100; i += ADAPTATION_STEP) {

    LINFOF("Going to check a ratio of %d", i);
    if (i != 0) {
      //place_all_pages(mem_segments, i);
    }

    //Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    best_stall_rate.at(BE) = std::min(best_stall_rate.at(BE),
                                      stall_rate.at(BE));

    //First check if we are violating the SLO
    if (stall_rate.at(HP) > target_stall_rate * 1.001) {

      LINFOF("SLO has been violated target: %.10lf, current: %.10lf",
             target_stall_rate, stall_rate.at(HP));
      if (i != 0) {
        LINFO("Going one step back before breaking!");
        //place_all_pages(mem_segments, (i - ADAPTATION_STEP));
        current_remote_ratio = i - ADAPTATION_STEP;
      } else {
        current_remote_ratio = i;
      }
      break;

    }

    //then check if there is any performance improvement for BE
    else if (stall_rate.at(BE) > best_stall_rate.at(BE) * 1.001
        || std::isnan(stall_rate.at(BE))) {

      LINFO("No performance improvement for the BE");
      if (i != 0) {
        LINFO("Going one step back before breaking!");
        //place_all_pages(mem_segments, (i - ADAPTATION_STEP));
        current_remote_ratio = i - ADAPTATION_STEP;
      } else {
        current_remote_ratio = i;
      }
      break;

    }

    //if performance improvement and no SLO violation continue climbing
    else {
      LINFO(
          "Performance improvement for BE without SLO Violation, continue climbing");
      LINFOF("best: %.10lf, current: %.10lf", best_stall_rate.at(BE),
             stall_rate.at(BE));
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
int release_mba(int optimal_mba, double target_stall_rate,
                int current_remote_ratio) {
  int i;
  std::vector<double> stall_rate(active_cpus);

  for (i = optimal_mba; i <= 100; i += 10) {

    if (i == 70 || i == 80)
      continue;

    apply_mba(i);
    //Measure the stall_rate of the applications
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);

    if (stall_rate.at(HP) > target_stall_rate && current_remote_ratio != 0) {
      LINFOF(
          "SLO violation has been detected (STOP releasing MBA): target: %.10lf, current: %.10lf",
          target_stall_rate, stall_rate.at(HP));
      optimal_mba = i;
      break;
    } else {
      LINFOF(
          "SLO violation has NOT been detected (CONTINUE releasing MBA): target: %.10lf, current: %.10lf",
          target_stall_rate, stall_rate.at(HP));
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
  int current_remote_ratio = 0;
  int optimal_mba = 100;
  double target_stall_rate;

  std::vector<double> stall_rate(active_cpus);
  std::vector<double> prev_stall_rate(active_cpus,
                                      std::numeric_limits<double>::infinity());

  LINFO("==============================================");
  LINFO("TESTING get_target_stall_rate function");
  LINFO("----------------------------------------------");
  target_stall_rate = get_target_stall_rate();
  LINFOF("Target SLO at this point: %.10lf", target_stall_rate);

  target_stall_rate = 0.001;  //some fake value
  //Measure the stall_rate of the applications
  stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                      _num_poll_outliers);

  LINFOF("Stall rate: target: %.10lf, current: %.10lf", target_stall_rate,
         stall_rate.at(HP));

  LINFO("==============================================");
  LINFO("TESTING search_optimal_mba function");
  LINFO("----------------------------------------------");
  optimal_mba = search_optimal_mba(target_stall_rate);

  target_stall_rate = 10.001;  //some fake value
  LINFO("==============================================");
  LINFO("TESTING apply_pagemigration_lr function");
  LINFO("----------------------------------------------");
  current_remote_ratio = apply_pagemigration_lr(target_stall_rate,
                                                current_remote_ratio);

  target_stall_rate = 0.001;  //some fake value
  LINFO("==============================================");
  LINFO("TESTING apply_pagemigration_rl function");
  LINFO("----------------------------------------------");
  current_remote_ratio = apply_pagemigration_rl(target_stall_rate,
                                                current_remote_ratio);

  target_stall_rate = 10.001;  //some fake value
  LINFO("==============================================");
  LINFO("TESTING release_mba function");
  LINFO("----------------------------------------------");
  optimal_mba = release_mba(optimal_mba, target_stall_rate,
                            current_remote_ratio);

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
 "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
 j, i, stall_rate.at(j), prev_stall_rate.at(j), best_stall_rate.at(j),
 interval_diff.at(j), minimum_interference.at(j));

 best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }

 // Assume App 0 is memory intensive (Best-Effort) and App 1 is compute intensive (High-Priority)!
 // First check if we are hurting the performance of the compute intensive app upto a certain percentage (5%)
 if (interval_diff.at(1) > minimum_interference.at(1)) {
 LINFO("Hmm...Is this really an interference?")
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
 "[Phase 1]: Exceeded the Minimal allowable interference for App 1, continue climbing!");
 break;
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
 "[Phase 2]: Minimal allowable interference for App 1 achieved, stop climbing!");
 break;
 }
 }

 else {
 LINFO(
 "[Phase 1 & 2]: Performance improvement for App 0 without interfering App 1, continue climbing");
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
 "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
 j, i, stall_rate.at(j), prev_stall_rate.at(j), best_stall_rate.at(j),
 interval_diff.at(j), minimum_interference.at(j));

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
 "App: %d MBA Level: %d StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
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
 "App: %d MBA level: %d StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
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
 "App: %d MBA level: %d StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
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
 "App: %d Ratio: %.2f StallRate: %1.10lf (previous %1.10lf; best %1.10lf) diff: %1.10lf noise: %1.10lf",
 j, i, stall_rate.at(j), prev_stall_rate.at(j), best_stall_rate.at(j),
 interval_diff.at(j), minimum_interference.at(j));

 best_stall_rate.at(j) = std::min(best_stall_rate.at(j), stall_rate.at(j));
 }

 // Assume App 0 is memory intensive (Best-Effort) and App 1 is compute intensive (High-Priority)!
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
