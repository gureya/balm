/*
 * MemorySegments.hpp
 *
 *  Created on: Sep 17, 2019
 *      Author: David Daharewa Gureya
 */

#ifndef TEST_BOOST_ASIO_SOCKETS_MEMORYSEGMENT_HPP_
#define TEST_BOOST_ASIO_SOCKETS_MEMORYSEGMENT_HPP_

#include <unistd.h>
#include <string>

namespace s11n_example {

struct memory_segment {
  //void* pageAlignedStartAddress;
  unsigned long pageAlignedLength;
  pid_t processID;

  template<typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    //ar & pageAlignedStartAddress;
    ar & pageAlignedLength;
    ar & processID;
  }
};

}  // namespace s11n_example

#endif /* TEST_BOOST_ASIO_SOCKETS_MEMORYSEGMENT_HPP_ */
