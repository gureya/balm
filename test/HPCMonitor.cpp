/*
 * =======================================================================================
 *
 *      Filename:  C-likwidAPI.c
 *
 *      Description:  Example how to use the LIKWID API in C/C++ applications
 *
 *      Version:   <VERSION>
 *      Released:  <DATE>
 *
 *      Author:  Thomas Gruber (tr), thomas.roehl@googlemail.com
 *      Project:  likwid
 *
 *      Copyright (C) 2015 RRZE, University Erlangen-Nuremberg
 *
 *      This program is free software: you can redistribute it and/or modify it under
 *      the terms of the GNU General Public License as published by the Free Software
 *      Foundation, either version 3 of the License, or (at your option) any later
 *      version.
 *
 *      This program is distributed in the hope that it will be useful, but WITHOUT ANY
 *      WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 *      PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along with
 *      this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * =======================================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <likwid.h>
#include <vector>

int err;
int* cpus;
int gid;
int active_cpus = 2;
char estr[] = "CPU_CLK_UNHALTED_CORE:FIXC1,RESOURCE_STALLS_ANY:PMC0";

//a function that starts counters
void start_counters() {
  //Start all counters in the previously set up event set.
  err = perfmon_startCounters();
  if (err < 0) {
    printf("Failed to start counters for group %d for thread %d\n", gid,
           (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    exit(-1);
    //return 1;
  }
}

//a function that stops counters
void stop_counters() {
  // Stop all counters in the previously started event set before doing a read.
  err = perfmon_stopCounters();
  if (err < 0) {
    printf("Failed to stop counters for group %d for thread %d\n", gid,
           (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    //return 1;
    exit(-1);
  }
}

int main(int argc, char* argv[]) {
  int i, j, k;
  double result = 0.0;

  char estr[] = "CPU_CLK_UNHALTED_CORE:FIXC1,RESOURCE_STALLS_ANY:PMC0";
  //perfmon_setVerbosity(3);
  // Load the topology module and print some values.
  err = topology_init();
  if (err < 0) {
    printf("Failed to initialize LIKWID's topology module\n");
    return 1;
  }
  // CpuInfo_t contains global information like name, CPU family, ...
  CpuInfo_t info = get_cpuInfo();
  // CpuTopology_t contains information about the topology of the CPUs.
  CpuTopology_t topo = get_cpuTopology();
  // Create affinity domains. Commonly only needed when reading Uncore counters
  affinity_init();

  printf("Likwid example on a %s with %d CPUs\n", info->name,
         topo->numHWThreads);

  cpus = (int*) malloc(active_cpus * sizeof(int));
  if (!cpus)
    return 1;

  for (i = 0; i < active_cpus; i++) {
    cpus[i] = topo->threadPool[i].apicId;
  }

  // Must be called before perfmon_init() but only if you want to use another
  // access mode as the pre-configured one. For direct access (0) you have to
  // be root.
  //accessClient_setaccessmode(0);

  // Initialize the perfmon module.
  err = perfmon_init(active_cpus, cpus);
  if (err < 0) {
    printf("Failed to initialize LIKWID's performance monitoring module\n");
    topology_finalize();
    return 1;
  }

  // Add eventset string to the perfmon module.
  gid = perfmon_addEventSet(estr);
  if (gid < 0) {
    printf(
        "Failed to add event string %s to LIKWID's performance monitoring module\n",
        estr);
    perfmon_finalize();
    topology_finalize();
    return 1;
  }

  // Setup the eventset identified by group ID (gid).
  err = perfmon_setupCounters(gid);
  if (err < 0) {
    printf(
        "Failed to setup group %d in LIKWID's performance monitoring module\n",
        gid);
    perfmon_finalize();
    topology_finalize();
    return 1;
  }
  // Start all counters in the previously set up event set.
  start_counters();

  static std::vector<double> prev_stalls(active_cpus, 0.0);
  static std::vector<double> prev_cycles(active_cpus, 0.0);

  std::vector<double> stalls(active_cpus, 0.0);
  std::vector<double> cycles(active_cpus, 0.0);
  std::vector<double> stall_rate(active_cpus);

  // Perform something
  for (k = 0; k < 3; k++) {
    sleep(2);
    // Stop all counters in the previously started event set.
    stop_counters();

    // Print the result of every thread/CPU for all events in estr.
    //Be sure to change this, currently supports only Intel
    int string_length = strlen(estr);
    char *intel_estr_temp = new char[string_length];
    //char intel_estr_temp[string_length];
    strcpy(intel_estr_temp, estr);

    char *ptr = strtok(intel_estr_temp, ",");
    j = 0;
    while (ptr != NULL) {
      for (i = 0; i < active_cpus; i++) {
        result = perfmon_getResult(gid, j, i);
        printf("Measurement result for event set %s at CPU %d: %f\n", ptr,
               cpus[i], result);
        if (j == 0) {
          cycles.at(i) = result;
        } else {
          stalls.at(i) = result;
        }
      }
      ptr = strtok(NULL, ",");
      j++;
    }

    for (i = 0; i < active_cpus; i++) {

      stall_rate.at(i) = ((double) (stalls.at(i) - prev_stalls.at(i)))
          / (cycles.at(i) - prev_cycles.at(i));
      printf("CPU: %d\n", cpus[i]);
      printf("cycles: %.0f prev_cycles: %.0f cycles - prev_cycles: %.0f\n",
             cycles.at(i), prev_cycles.at(i),
             (cycles.at(i) - prev_cycles.at(i)));
      printf("stalls: %.0f prev_stalls: %.0f stalls - prev_stalls: %.0f\n",
             stalls.at(i), prev_stalls.at(i),
             (stalls.at(i) - prev_stalls.at(i)));
      printf("stall_rate: %.10f\n", stall_rate.at(i));
      printf(
          "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    }
    printf(
        "================================================================\n");

    for (i = 0; i < active_cpus; i++) {
      prev_stalls.at(i) = stalls.at(i);
      prev_cycles.at(i) = cycles.at(i);
    }

    start_counters();
  }

  free(cpus);
  // Uninitialize the perfmon module.
  perfmon_finalize();
  affinity_finalize();
  // Uninitialize the topology module.
  topology_finalize();
  return 0;
}

