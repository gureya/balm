#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <likwid.h>

static int sleeptime = 5;

static int run = 1;

void INThandler(int sig) {
  signal(sig, SIG_IGN);
  run = 0;
}

int main(int argc, char* argv[]) {
  int i, c, err = 0;
  double timer = 0.0;
  topology_init();
  numa_init();
  affinity_init();
  timer_init();
  CpuInfo_t cpuinfo = get_cpuInfo();
  CpuTopology_t cputopo = get_cpuTopology();
  int numCPUs = cputopo->activeHWThreads;
  int* cpus = malloc(numCPUs * sizeof(int));
  if (!cpus) {
    affinity_finalize();
    numa_finalize();
    topology_finalize();
    return 1;
  }
  c = 0;
  for (i = 0; i < cputopo->numHWThreads; i++) {
    if (cputopo->threadPool[i].inCpuSet) {
      cpus[c] = cputopo->threadPool[i].apicId;
      c++;
    }
  }
  NumaTopology_t numa = get_numaTopology();
  AffinityDomains_t affi = get_affinityDomains();
  timer = timer_getCpuClock();
  perfmon_init(numCPUs, cpus);
  int gid1 = perfmon_addEventSet("RESOURCE_STALLS");
  if (gid1 < 0) {
    printf("Failed to add performance group RESOURCE_STALLS\n");
    err = 1;
    goto monitor_exit;
  }
  signal(SIGINT, INThandler);

  while (run) {
    perfmon_setupCounters(gid1);
    perfmon_startCounters();
    sleep(sleeptime);
    perfmon_stopCounters();
    for (c = 0; c < 4; c++) {
      for (i = 0; i < perfmon_getNumberOfMetrics(gid1); i++) {
        printf("%s,cpu=%d %f\n", perfmon_getMetricName(gid1, i), cpus[c],
               perfmon_getLastMetric(gid1, i, c));
      }
    }

    printf("==========================================\n");

  }

  monitor_exit: free(cpus);
  perfmon_finalize();
  affinity_finalize();
  numa_finalize();
  topology_finalize();
  return 0;
}
