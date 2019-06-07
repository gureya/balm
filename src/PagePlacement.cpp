/*
 * PagePlacement.cpp
 *
 *  Created on: Jun 7, 2019
 *      Author: David Daharewa Gureya
 */

#include "include/BwManager.hpp"
#include "include/PagePlacement.hpp"
#include "include/Logger.hpp"

static int pagesize;

void place_all_pages(std::vector<MySharedMemory> mem_segments, double r) {
  for (int i = 0; i < mem_segments.size(); i++) {
    move_pages_remote(mem_segments.at(i).processID,
                      mem_segments.at(i).pageAlignedStartAddress,
                      mem_segments.at(i).pageAlignedLength, r);
  }
}

//place pages with the move_pages system call
//courtesy: https://stackoverflow.com/questions/10989169/numa-memory-page-migration-overhead/11148999
void move_pages_remote(pid_t pid, void *start, unsigned long len,
                       double remote_ratio) {

  pagesize = numa_pagesize();

  char *pages;

  int i, rc;

  void **addr;
  int *status;
  int *nodes;

  int page_count = len / pagesize;

  double interleaved_pages;
  int remote_node, local_node;

  addr = (void **) malloc(sizeof(char *) * page_count);
  status = (int *) malloc(page_count * sizeof(int *));
  nodes = (int *) malloc(page_count * sizeof(int *));

  if (!start || !addr || !status || !nodes) {
    LINFO("Unable to allocate memory");
    exit(1);
  }

  pages = (char *) start;

  //set the remote and local nodes here
  if (WORKER_NODE == 2) {
    remote_node = 0;
    local_node = 0;
  } else {
    remote_node = 0;
    local_node = 0;
  }

  //uniform distribution memory allocation (using the bwap style format)
  if (remote_ratio <= 50) {
    interleaved_pages = (remote_ratio / 100 * (double) page_count) * MAX_NODES;
    //LINFOF("page_count:%d interleaved_pages:%d", page_count, (int) interleaved_pages);
    for (i = 0; i < page_count; ++i) {
      addr[i] = pages + i * pagesize;
      if (i < interleaved_pages) {
        if (i % 2 == 0) {
          nodes[i] = local_node;
        } else {
          nodes[i] = remote_node;
        }
      } else {
        nodes[i] = local_node;
      }
      status[i] = -123;
    }

  } else {
    interleaved_pages = ((100 - remote_ratio) / 100 * (double) page_count)
        * MAX_NODES;
    //LINFOF("page_count:%d interleaved_pages:%d", page_count, (int) interleaved_pages);
    for (i = 0; i < page_count; ++i) {
      addr[i] = pages + i * pagesize;
      if (i < interleaved_pages) {
        if (i % 2 == 0) {
          nodes[i] = local_node;
        } else {
          nodes[i] = remote_node;
        }
      } else {
        nodes[i] = remote_node;
      }
      status[i] = -123;
    }
  }

  rc = move_pages(pid, page_count, addr, nodes, status, MPOL_MF_MOVE);
  if (rc < 0 && errno != ENOENT) {
    perror("move_pages");
    exit(1);
  }

  free(addr);
  free(status);
  free(nodes);

}

