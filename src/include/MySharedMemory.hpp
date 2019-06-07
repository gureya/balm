/*
 * MySharedMemory.hpp
 *
 *  Created on: Jun 7, 2019
 *      Author: David Daharewa Gureya
 */

#ifndef INCLUDE_MYSHAREDMEMORY_HPP_
#define INCLUDE_MYSHAREDMEMORY_HPP_

#include <unistd.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include "include/Logger.hpp"

namespace ipc = boost::interprocess;

class MySharedMemory {
 public:
  void* pageAlignedStartAddress;
  unsigned long pageAlignedLength;
  pid_t processID;

  //constructor
  MySharedMemory(void* start, unsigned long len, pid_t pid);
};

std::vector<MySharedMemory> get_shared_memory();
void destroy_shared_memory();
void test();

#endif /* INCLUDE_MYSHAREDMEMORY_HPP_ */
