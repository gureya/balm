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

void move_pages_remote(pid_t pid, void *addr, unsigned long len, double ratio);
void place_all_pages(std::vector<MySharedMemory> mem_segments, double ratio);

#endif /* INCLUDE_PAGEPLACEMENT_HPP_ */
