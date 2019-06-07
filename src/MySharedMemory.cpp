/*
 * MySharedMemory.cpp
 *
 *  Created on: Jun 7, 2019
 *      Author: David Daharewa Gureya
 */

#include "include/MySharedMemory.hpp"

MySharedMemory::MySharedMemory(void* start, unsigned long len, pid_t pid) {
  pageAlignedStartAddress = start;
  pageAlignedLength = len;
  processID = pid;
}

std::vector<MySharedMemory> get_shared_memory() {

  std::vector<MySharedMemory> mem_segments;

  try {
    //A special shared memory where we can
    //construct objects associated with a name.
    //Connect to the already created shared memory segment
    //and initialize needed resources
    ipc::managed_shared_memory segment(ipc::open_only, "MySharedMemory");  //segment name

    //Alias an STL compatible allocator of ints that allocates ints from the managed
    //shared memory segment.  This allocator will allow to place containers
    //in managed shared memory segments
    typedef ipc::allocator<MySharedMemory,
        ipc::managed_shared_memory::segment_manager> ShmemAllocator;

    //Alias a vector that uses the previous STL-like allocator
    typedef ipc::vector<MySharedMemory, ShmemAllocator> MyVector;

    //Find the vector using the c-string name
    MyVector *myvector = segment.find<MyVector>("MyVector").first;

    //print the segments
    for (int i = 0; i < myvector->size(); i++) {
      MySharedMemory sharedmemory(myvector->at(i).pageAlignedStartAddress,
                                  myvector->at(i).pageAlignedLength,
                                  myvector->at(i).processID);
      mem_segments.push_back(sharedmemory);
    }

    //When done, destroy the vector from the segment
    segment.destroy<MyVector>("MyVector");
  } catch (...) {
    ipc::shared_memory_object::remove("MySharedMemory");
    throw;
  }

  return mem_segments;
}

void destroy_shared_memory() {
  //When done, remove the segment
  ipc::shared_memory_object::remove("MySharedMemory");
  LINFO("MySharedMemory has been destroyed!");
}

