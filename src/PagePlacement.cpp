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
    /*move_pages_remote(mem_segments.at(i).processID,
     mem_segments.at(i).pageAlignedStartAddress,
     mem_segments.at(i).pageAlignedLength, r);*/
    dombind(mem_segments.at(i).pageAlignedStartAddress,
            mem_segments.at(i).pageAlignedLength, r);
  }
}

// interleave pages using the weights with the mbind() system call
void dombind(void *addr, unsigned long len, double remote_ratio) {

  size_t size = len;
  void *start = addr;
  int i;
  pagesize = numa_pagesize();

  double weights[MAX_NODES];  // the weights should be in ascending order

  if (remote_ratio <= 50) {
    weights[0] = remote_ratio;
    weights[1] = 100 - remote_ratio;
  } else {
    weights[0] = 100 - remote_ratio;
    weights[1] = remote_ratio;
  }

  // nodes that can still receive pages
  struct bitmask *node_set = numa_bitmask_alloc(MAX_NODES);  // numa_allocate_nodemask();
  numa_bitmask_setall(node_set);

  float w = 0;  // weight that has already been allocated among the nodes that can still receive pages
  int a = MAX_NODES;  // number of nodes which can still receive pages

  size_t total_size = 0;  // total size interleaved so far
  size_t my_size = 0;

  size_t remaining_a;

  for (i = 0; i < MAX_NODES; ++i) {
    if (total_size == size) {
      break;
    }

    // b = size that remains to allocate in the next node with smallest beta
    float b = weights[i] - w;
    printf("i: %d\tb: %.2f\ta:%d\n", i, b, a);

    my_size = a * (b / 100) * size;
    printf("my_size_a: %ld\n", my_size);

    // round up to multiple of the page size
    my_size = PAGE_ALIGN_UP(my_size);

    remaining_a = size - total_size;
    if (my_size > remaining_a) {
      my_size = remaining_a;
    }

    total_size += my_size;
    printf("my_size_b: %zu\n", my_size);

    // only interleave if memory is in the region
    if (my_size != 0) {
      DIEIF(
          mbind(start, my_size, MPOL_INTERLEAVE, node_set->maskp, node_set->size + 1, MPOL_MF_MOVE_ALL | MPOL_MF_STRICT) != 0,
          "mbind interleave failed");
    }

    start =
        reinterpret_cast<void*>(reinterpret_cast<intptr_t>(start) + my_size);  // increment base to a new location
    a--;  // one less node where to allocate pages
    w = weights[i];  // we update the size already allocated in the remaining nodes
    if (numa_bitmask_isbitset(node_set, i)) {
      numa_bitmask_clearbit(node_set, i);  // remove node i from the set of remaining nodes
    }
  }

  numa_bitmask_free(node_set);
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

