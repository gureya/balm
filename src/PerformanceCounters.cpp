/*
 * performancecounters.cpp
 *
 *  Created on: May 31, 2019
 *      Author: david
 */
#include "include/PerformanceCounters.hpp"

static bool initiatialized = false;

/*
 * A function that uses the likwid library to measure the stall rates
 * Credits: https://github.com/RRZE-HPC/likwid/blob/master/examples/C-likwidAPI.c
 *
 * On AMD we use the following counters
 * EventSelect 0D1h Dispatch Stalls: The number of processor cycles where the decoder
 * is stalled for any reason (has one or more instructions ready but can't dispatch
 * them due to resource limitations in execution)
 * &
 * EventSelect 076h CPU Clocks not Halted: The number of clocks that the CPU is not in a halted state.
 *
 * On Intel we use the following counters
 * RESOURCE_STALLS: Cycles Allocation is stalled due to Resource Related reason
 * &
 * UNHALTED_CORE_CYCLES:  Count core clock cycles whenever the clock signal on the specific
 * core is running (not halted)
 *
 */
int err;
int* cpus;
int gid;
CpuInfo_t info;
static int nnodes;
static int ncpus_per_node;
static int ncpus;

//list of all the events for the different architectures supported
//char amd_estr[] = "CPU_CLOCKS_UNHALTED:PMC0,DISPATCH_STALLS:PMC1"; //AMD
//char amd_estr[] = "DISPATCH_STALLS:PMC0";  //AMD DISPATCH_STALL_LDQ_FULL,DISPATCH_STALL_FP_SCHED_Q_FULL
char const *amd_estr = "DISPATCH_STALLS:PMC0";
//char amd_estr[] = "DISPATCH_STALL_INSTRUCTION_RETIRED_Q_FULL:PMC0";
//char intel_estr[] =
//    "CPU_CLOCK_UNHALTED_THREAD_P:PMC0,RESOURCE_STALLS_ANY:PMC1"; //Intel Broadwell EP
//char intel_estr[] = "RESOURCE_STALLS_ANY:PMC0";  //Intel Broadwell EP, Intel Core Westmere processor
char const *intel_estr = "RESOURCE_STALLS_ANY:PMC0";
//if a specific pmc has been specified override the above variables!

void initialize_likwid() {
  if (!initiatialized) {

    //perfmon_setVerbosity(3);
    //Load the topology module and print some values.
    err = topology_init();
    if (err < 0) {
      LDEBUG("Failed to initialize LIKWID's topology module\n");
      //return 1;
      exit(-1);
    }
    // CpuInfo_t contains global information like name, CPU family, ...
    //CpuInfo_t info = get_cpuInfo();
    info = get_cpuInfo();
    // CpuTopology_t contains information about the topology of the CPUs.
    CpuTopology_t topo = get_cpuTopology();
    // Create affinity domains. Commonly only needed when reading Uncore counters
    affinity_init();

    LINFOF("Likwid Measuremennts on a %s with %d CPUs\n", info->name,
           topo->numHWThreads);

    ncpus = topo->numHWThreads;
    nnodes = numa_num_configured_nodes();
    ncpus_per_node = ncpus / nnodes;

    //active_cpus = OPT_NUM_WORKERS_VALUE * ncpus_per_node;
    // is currently the size of the vector
    active_cpus = BWMAN_CORES.size();

    LINFOF(
        "| [NODES] - %d: [CPUS] - %d: [CPUS_PER_NODE] - %d: [ACTIVE_CPUS] - %d |\n",
        nnodes, ncpus, ncpus_per_node, active_cpus);

    //cpus = (int*) malloc(topo->numHWThreads * sizeof(int));
    //for now only monitor one CPU
    cpus = (int*) malloc(active_cpus * sizeof(int));

    if (!cpus)
      exit(-1);   //return 1;

    //set the monitoring core
    for (int i = 0; i < active_cpus; i++) {
      cpus[i] = BWMAN_CORES.at(i);
      //cpus[i] = topo->threadPool[i].apicId;
    }

    // Must be called before perfmon_init() but only if you want to use another
    // access mode as the pre-configured one. For direct access (0) you have to
    // be root.
    //accessClient_setaccessmode(0);
    // Initialize the perfmon module.
    //err = perfmon_init(topo->numHWThreads, cpus);
    err = perfmon_init(active_cpus, cpus);
    if (err < 0) {
      LDEBUG("Failed to initialize LIKWID's performance monitoring module\n");
      topology_finalize();
      //return 1;
      exit(-1);
    }

    /*
     * pick the right event based on the architecture,
     * currently tested on AMD {amd64_fam15h_interlagos && amd64_fam10h_istanbul}
     * and INTEL {Intel Broadwell EP}
     * uses a simple flag to do this, may use the more accurate cpu names or families
     *
     */
    LINFOF("Short name of the CPU: %s\n", info->short_name);
    LINFOF("Intel flag: %d\n", info->isIntel);
    LINFOF("CPU family ID: %" PRIu32 "\n", info->family);
    // Add eventset string to the perfmon module.
    //Intel CPU's
    if (info->isIntel == 1) {
      LINFOF("Setting up events %s for %s\n", intel_estr, info->short_name);
      gid = perfmon_addEventSet(intel_estr);
    }
    //for AMD!
    else if (info->isIntel == 0) {
      LINFOF("Setting up events %s for %s\n", amd_estr, info->short_name);
      gid = perfmon_addEventSet(amd_estr);
    } else {
      LINFO("Unsupported Architecture at the moment\n");
      exit(-1);
    }

    if (gid < 0) {
      LDEBUGF(
          "Failed to add event string %s to LIKWID's performance monitoring module\n",
          intel_estr);
      perfmon_finalize();
      topology_finalize();
      //return 1;
      exit(-1);
    }

    // Setup the eventset identified by group ID (gid).
    err = perfmon_setupCounters(gid);
    if (err < 0) {
      LDEBUGF(
          "Failed to setup group %d in LIKWID's performance monitoring module\n",
          gid);
      perfmon_finalize();
      topology_finalize();
      //return 1;
      exit(-1);
    }

    // Start all counters in the previously set up event set.
    err = perfmon_startCounters();
    if (err < 0) {
      LDEBUGF("Failed to start counters for group %d for thread %d\n", gid,
              (-1 * err) - 1);
      perfmon_finalize();
      topology_finalize();
      exit(-1);
      //return 1;
    }
    initiatialized = true;
    //printf("Setting up Likwid statistics for the first time\n");
  }

}

std::vector<double> get_stall_rate() {
  int i;
  double result = 0.0;

  //static double prev_cycles = 0;
  //static double prev_stalls = 0;
  static uint64_t prev_clockcounts = 0;
  static std::vector<double> prev_stalls(active_cpus, 0.0);

  std::vector<double> stalls(active_cpus, 0.0);
  std::vector<double> stall_rate(active_cpus);

  // Stop all counters in the previously started event set before doing a read.
  err = perfmon_stopCounters();
  if (err < 0) {
    LDEBUGF("Failed to stop counters for group %d for thread %d\n", gid,
            (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    //return 1;
    exit(-1);
  }

  // Read the result of every active thread/CPU for all events in estr.

  for (i = 0; i < active_cpus; i++) {
    result = perfmon_getResult(gid, 0, i);
    stalls.at(i) = result;
    //printf("Measurement result at CPU %d: %f\n", cpus[i], result);
  }

  uint64_t clock = readtsc();  // read clock

  for (i = 0; i < active_cpus; i++) {

    stall_rate.at(i) = ((double) (stalls.at(i) - prev_stalls.at(i)))
        / (clock - prev_clockcounts);

    //stall_rate.at(i) = ((double) (stalls.at(i) - prev_stalls.at(i)));

    /*printf(
     "clock: %" PRIu64 " prev_clockcounts: %" PRIu64 " clock - prev_clockcounts: %" PRIu64 "\n",
     clock, prev_clockcounts, (clock - prev_clockcounts));
     printf("stalls: %.0f prev_stalls: %.0f stalls - prev_stalls: %.0f\n",
     stalls.at(i), prev_stalls.at(i), (stalls.at(i) - prev_stalls.at(i)));
     printf("stall_rate: %.10f\n", stall_rate.at(i));*/
  }
  //printf("================================================================\n");

  //prev_cycles = cycles;
  for (i = 0; i < active_cpus; i++) {
    prev_stalls.at(i) = stalls.at(i);
  }

  prev_clockcounts = clock;

  err = perfmon_startCounters();
  if (err < 0) {
    LDEBUGF("Failed to start counters for group %d for thread %d\n", gid,
            (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    exit(-1);
    //return 1;
  }

  return stall_rate;
  //return stalls;
}

void stop_all_counters() {
  err = perfmon_stopCounters();
  if (err < 0) {
    LDEBUGF("Failed to stop counters for group %d for thread %d\n", gid,
            (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    //return 1;
    exit(-1);
  }
  free(cpus);
  // Uninitialize the perfmon module.
  perfmon_finalize();
  affinity_finalize();
  // Uninitialize the topology module.
  topology_finalize();
  LINFO("All counters have been stopped\n");
}

// samples stall rate multiple times and filters outliers
std::vector<double> get_average_stall_rate(int num_measurements,
                                           useconds_t usec_between_measurements,
                                           int num_outliers_to_filter) {
  //return 0.0;
  std::vector<std::vector<double> > measurements(
      active_cpus, std::vector<double>(num_measurements));

  std::vector<double> stall_rate;

  //throw away a measurement, just because
  get_stall_rate();
  usleep(usec_between_measurements);

  // do N measurements, T usec apart
  int j, i;
  for (i = 0; i < num_measurements; i++) {
    stall_rate = get_stall_rate();
    for (j = 0; j < active_cpus; j++) {
      measurements.at(j).at(i) = stall_rate.at(j);
    }
    usleep(usec_between_measurements);
  }

  //for debugging purposes!!
  /*printf("Before Sorting\n");
   for (j = 0; j < active_cpus; j++) {
   printf("Measurements for CPU %d: ", j);
   for (i = 0; i < num_measurements; i++) {
   printf("%d: %1.10lf ", i, measurements.at(j).at(i));
   }
   printf("\n");
   }
   printf("\n");*/

  // filter outliers
  std::vector<double> average_stall_rate(active_cpus);

  for (j = 0; j < active_cpus; j++) {
    std::sort(measurements.at(j).begin(), measurements.at(j).end());
    measurements.at(j).erase(measurements.at(j).end() - num_outliers_to_filter,
                             measurements.at(j).end());
    measurements.at(j).erase(
        measurements.at(j).begin(),
        measurements.at(j).begin() + num_outliers_to_filter);

    double sum = std::accumulate(measurements.at(j).begin(),
                                 measurements.at(j).end(), 0.0);
    average_stall_rate.at(j) = sum / measurements.at(j).size();
  }

  //for debugging purposes!!
  /* printf("After Sorting\n");
   for (j = 0; j < active_cpus; j++) {
   printf("Measurements for App %d: ", j);
   for (i = 0; i < (num_measurements - (num_outliers_to_filter * 2)); i++) {
   printf("%d: %1.10lf ", i, measurements.at(j).at(i));
   }
   printf("\n");
   }
   printf("\n");*/

  // return the average stall rate in a vector
  return average_stall_rate;
}

#if defined(__unix__) || defined(__linux__)
// System-specific definitions for Linux

// read time stamp counter
inline uint64_t readtsc(void) {
  uint32_t lo, hi;
  __asm __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi) : : );
  return lo | (uint64_t) hi << 32;
}

// read performance monitor counter
inline uint64_t readpmc(int32_t n) {
  uint32_t lo, hi;
  __asm __volatile__ ("rdpmc" : "=a"(lo), "=d"(hi) : "c"(n) : );
  return lo | (uint64_t) hi << 32;
}

#else  // not Linux

#error We only support Linux

#endif

