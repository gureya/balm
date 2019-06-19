/*
 * PagePlacement.hpp
 *
 *  Created on: Jun 7, 2019
 *      Author: David Daharewa Gureya
 */

#ifndef INCLUDE_PAGEPLACEMENT_HPP_
#define INCLUDE_PAGEPLACEMENT_HPP_

#include <unistd.h>
#include <numaif.h>
#include <numa.h>

#include <sys/syscall.h>
#include <errno.h>

#include "include/MySharedMemory.hpp"

#define PAGE_ALIGN_DOWN(x) (((intptr_t) (x)) & PAGE_MASK)
#define PAGE_ALIGN_UP(x) ((((intptr_t) (x)) + ~PAGE_MASK) & PAGE_MASK)

static const int PAGE_SIZE = sysconf(_SC_PAGESIZE);
static const int PAGE_MASK = (~(PAGE_SIZE - 1));

void move_pages_remote(pid_t pid, void *addr, unsigned long len, double ratio);
void move_pages_remote(pid_t pid, void *addr, unsigned long len);
void place_all_pages(std::vector<MySharedMemory> mem_segments, double ratio);
void place_all_pages(std::vector<MySharedMemory> mem_segments);  //initial

#endif /* INCLUDE_PAGEPLACEMENT_HPP_ */
