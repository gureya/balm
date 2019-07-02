/*
 * PagePlacement.cpp
 *
 *  Created on: Jun 7, 2019
 *      Author: David Daharewa Gureya
 */

#include "include/BwManager.hpp"
#include "include/PagePlacement.hpp"
#include "include/Logger.hpp"

#include <numeric> //vector sum

static int pagesize;
//bool weight_initialized = false;

//temporary vector of weights initialized to zero
std::vector<std::pair<double, int>> BWMAN_WEIGHTS_temp(MAX_NODES,
                                                       std::make_pair(0, 0));

int check_sum(std::vector<std::pair<double, int>> n) {
  double sum = 0;
  int i = 0;

  for (i = 0; i < MAX_NODES; i++) {
    sum += n.at(i).first;
  }
  return std::lround(sum);
}

// Calculate the new weights with respect to the new ratio!
void get_new_weights(double s) {

  int i = 0;
  double new_s = 0;
  new_s = sum_ww + s;
  double sum = 0;

  for (i = 0; i < MAX_NODES; i++) {
    switch (BWMAN_WORKERS) {
      case 1:
        // workers: 0
        if (BWMAN_WEIGHTS.at(i).second == 0) {
          BWMAN_WEIGHTS_temp.at(i).second = BWMAN_WEIGHTS.at(i).second;
          BWMAN_WEIGHTS_temp.at(i).first = round(
              (BWMAN_WEIGHTS.at(i).first / sum_ww * new_s) * 10) / 10;
          sum += BWMAN_WEIGHTS_temp.at(i).first;
        } else {
          BWMAN_WEIGHTS_temp.at(i).second = BWMAN_WEIGHTS.at(i).second;
          BWMAN_WEIGHTS_temp.at(i).first = round(
              (BWMAN_WEIGHTS.at(i).first / sum_nww * (100 - new_s)) * 10) / 10;
          sum += BWMAN_WEIGHTS_temp.at(i).first;
        }
        break;
      case 2:
        // workers: 0, 1
        if (BWMAN_WEIGHTS.at(i).second == 0
            || BWMAN_WEIGHTS.at(i).second == 1) {
          BWMAN_WEIGHTS_temp.at(i).second = BWMAN_WEIGHTS.at(i).second;
          BWMAN_WEIGHTS_temp.at(i).first = round(
              (BWMAN_WEIGHTS.at(i).first / sum_ww * new_s) * 10) / 10;
          sum += BWMAN_WEIGHTS_temp.at(i).first;
        } else {
          BWMAN_WEIGHTS_temp.at(i).second = BWMAN_WEIGHTS.at(i).second;
          BWMAN_WEIGHTS_temp.at(i).first = round(
              (BWMAN_WEIGHTS.at(i).first / sum_nww * (100 - new_s)) * 10) / 10;
          sum += BWMAN_WEIGHTS_temp.at(i).first;
        }
        break;
      case 3:
        // workers: 1,2,3
        if (BWMAN_WEIGHTS.at(i).second == 1 || BWMAN_WEIGHTS.at(i).second == 2
            || BWMAN_WEIGHTS.at(i).second == 3) {
          BWMAN_WEIGHTS_temp.at(i).second = BWMAN_WEIGHTS.at(i).second;
          BWMAN_WEIGHTS_temp.at(i).first = round(
              (BWMAN_WEIGHTS.at(i).first / sum_ww * new_s) * 10) / 10;
          sum += BWMAN_WEIGHTS_temp.at(i).first;
        } else {
          BWMAN_WEIGHTS_temp.at(i).second = BWMAN_WEIGHTS.at(i).second;
          BWMAN_WEIGHTS_temp.at(i).first = round(
              (BWMAN_WEIGHTS.at(i).first / sum_nww * (100 - new_s)) * 10) / 10;
          sum += BWMAN_WEIGHTS_temp.at(i).first;
        }
        break;
      case 4:
        // workers: 0,1,2,3
        if (BWMAN_WEIGHTS.at(i).second == 0 || BWMAN_WEIGHTS.at(i).second == 1
            || BWMAN_WEIGHTS.at(i).second == 2
            || BWMAN_WEIGHTS.at(i).second == 3) {
          BWMAN_WEIGHTS_temp.at(i).second = BWMAN_WEIGHTS.at(i).second;
          BWMAN_WEIGHTS_temp.at(i).first = round(
              (BWMAN_WEIGHTS.at(i).first / sum_ww * new_s) * 10) / 10;
          sum += BWMAN_WEIGHTS_temp.at(i).first;
        } else {
          BWMAN_WEIGHTS_temp.at(i).second = BWMAN_WEIGHTS.at(i).second;
          BWMAN_WEIGHTS_temp.at(i).first = round(
              (BWMAN_WEIGHTS.at(i).first / sum_nww * (100 - new_s)) * 10) / 10;
          sum += BWMAN_WEIGHTS_temp.at(i).first;
        }
        break;
      default:
        LINFOF("Sorry, %d Worker nodes is not supported at the moment!\n",
               BWMAN_WORKERS)
        ;
        exit(-1);
    }
  }

  //sort the vector in ascending order just incase it gets unordered
  sort(BWMAN_WEIGHTS_temp.begin(), BWMAN_WEIGHTS_temp.end());

  /*printf("%.2f\n", sum);

   printf("New Weights: \t");
   for (i = 0; i < MAX_NODES; i++) {
   printf("%d : %.2f\t", BWMAN_WEIGHTS_temp.at(i).second,
   BWMAN_WEIGHTS_temp.at(i).first);
   }
   printf("\n");*/

  if ((check_sum(BWMAN_WEIGHTS_temp)) != 100) {
    printf("**Sum of New weights must be equal to 100, sum=%d!**\n",
           check_sum(BWMAN_WEIGHTS_temp));
    exit(-1);
  }

  /*printf(
   "===========================================================================\n");*/
}

//debugging function
void get_node_mappings(int page_count, int *nodes) {

  //Test weights are reflected on the node mappings!
  std::vector<int> node_count(MAX_NODES, 0);
  int i;
  for (i = 0; i < page_count; i++) {
    int node_number = nodes[i];
    if (node_number == 0) {
      node_count.at(0) += 1;
    } else if (node_number == 1) {
      node_count.at(1) += 1;
    } else if (node_number == 2) {
      node_count.at(2) += 1;
    } else if (node_number == 3) {
      node_count.at(3) += 1;
    } else if (node_number == 4) {
      node_count.at(4) += 1;
    } else if (node_number == 5) {
      node_count.at(5) += 1;
    } else if (node_number == 6) {
      node_count.at(6) += 1;
    } else if (node_number == 7) {
      node_count.at(7) += 1;
    } else {
      printf("Invalid Node Number %d", node_number);
    }
  }

  int sum_of_elems = std::accumulate(node_count.begin(), node_count.end(), 0);
  printf("Total pages: %d\n", sum_of_elems);
  double sum = 0.0;
  for (i = 0; i < MAX_NODES; i++) {
    double weight = (((double) node_count.at(i) / (double) sum_of_elems) * 100);
    printf("Node %d count %d Weight %.2f\n", i, node_count.at(i), weight);
    sum += weight;
  }
  printf("Total Weight: %.2f\n", sum);

}

void place_all_pages(std::vector<MySharedMemory> mem_segments, double r) {
  get_new_weights(r);
#pragma omp parallel for
  for (size_t i = 0; i < mem_segments.size(); i++) {
    move_pages_remote(mem_segments.at(i).processID,
                      mem_segments.at(i).pageAlignedStartAddress,
                      mem_segments.at(i).pageAlignedLength, r);
  }
  //weight_initialized = false;
}

//initial page placement with weighted interleave
/*void place_all_pages(std::vector<MySharedMemory> mem_segments) {
 for (size_t i = 0; i < mem_segments.size(); i++) {
 move_pages_remote(mem_segments.at(i).processID,
 mem_segments.at(i).pageAlignedStartAddress,
 mem_segments.at(i).pageAlignedLength);
 }
 }*

 //place pages with the move_pages system call
 //courtesy: https://stackoverflow.com/questions/10989169/numa-memory-page-migration-overhead/11148999
 void move_pages_remote(pid_t pid, void *start, unsigned long len,
 double remote_ratio) {

 pagesize = numa_pagesize();

 //LINFOF("numa_pagesize: %d", pagesize);

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
 if (BWMAN_WORKERS == 2) {
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
 }*/

//initial page placement with weighted interleave
void move_pages_remote(pid_t pid, void *start, unsigned long len,
                       double remote_ratio) {

  /*if (!weight_initialized) {
   get_new_weights(remote_ratio);
   weight_initialized = true;
   }*/

  pagesize = numa_pagesize();

  char *pages;

  int i, j, rc;

  void **addr;
  int *status;
  int *nodes;

  int page_count = len / pagesize;

  addr = (void **) malloc(sizeof(char *) * page_count);
  status = (int *) malloc(page_count * sizeof(int *));
  nodes = (int *) malloc(page_count * sizeof(int *));

  if (!start || !addr || !status || !nodes) {
    LINFO("Unable to allocate memory");
    exit(1);
  }

  pages = (char *) start;

  //uniform distribution memory allocation (using the bwap style format)
  //first set the page addresses, openmp for faster processing
//#pragma omp parallel for firstprivate(pages,pagesize)
  for (i = 0; i < page_count; i++) {
    addr[i] = pages + i * pagesize;
    nodes[i] = 0;  //incase the last page is not initialized
  }

  //set the page distribution using a weighted version
  double i_p;  //interleaved_pages
  double w = 0;  // weight that has already been allocated among the nodes that can still receive pages
  int a = MAX_NODES;  // number of nodes which can still receive pages
  int i_k = 0;  //lower_bound for the pages
  int r_pages = 0;  //remaining pages
  int my_node;  //the node of a page

  //create a vector of node id's
  std::vector<int> node_ids;
  for (i = 0; i < MAX_NODES; i++) {
    node_ids.push_back(BWMAN_WEIGHTS_temp.at(i).second);
  }

  for (i = 0; i < MAX_NODES; i++) {

    double b = BWMAN_WEIGHTS_temp.at(i).first - w;
    i_p = a * (b / 100) * page_count;

    r_pages = page_count - i_k;
    if (i_p > r_pages) {
      i_p = r_pages;
    }

    /*for (int k = 0; k < a; k++) {
     printf("node id: %d\t", node_ids.at(k));
     }
     printf("\n");

     printf("i: %d b: %.2f i_p: %.2f i_k: %d end: %d a: %d r_pages: %d\n", i, b,
     i_p, i_k, (i_k + (int) i_p), a, r_pages);*/

    if (i_k == page_count) {
      break;
    }

    if (i_p != 0) {
      int upper_bound = i_k + i_p;
//#pragma omp parallel for firstprivate(my_node,a)
      for (j = i_k; j < upper_bound; j++) {
        my_node = j % a;
        nodes[j] = node_ids.at(my_node);
      }
    }

    node_ids.erase(node_ids.begin());
    a--;
    w = BWMAN_WEIGHTS_temp.at(i).first;
    i_k += i_p;

  }

  /*for (i = 0; i < page_count; i++) {
   printf("%d: %d\n", i, nodes[i]);
   }*/

  /*for (i = 0; i < page_count; i++) {
   if (nodes[i] < 0 || nodes[i] > MAX_NODES) {
   printf("Page %d node %d", i, nodes[i]);
   exit(1);
   }
   }*/

  //get_node_mappings(page_count, nodes);
  rc = move_pages(pid, page_count, addr, nodes, status, MPOL_MF_MOVE);
  if (rc < 0 && errno != ENOENT) {
    perror("move_pages");
    exit(1);
  }

  free(addr);
  free(status);
  free(nodes);
}

