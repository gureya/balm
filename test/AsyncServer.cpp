/*
 * AsyncServer.cpp
 *
 *  Created on: Sep 12, 2019
 *      Author: David Daharewa Gureya
 */

#include "ConnectionHandler.hpp"

int main(int argc, char *argv[]) {
  try {
    boost::asio::io_service io_service;
    Server server(io_service);
    io_service.run();
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

//If we want it to stop, then we can do the following:
  /* boost::optional<boost::asio::io_service::work> work = boost::in_place(
   boost::ref(io_service));
   work = boost::none;*/

  return 0;
}

