//
// client.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
// compile the code as stated below^M
// g++ poll_n_clients.cpp -o poll_n_clients -lboost_system -lpthread

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

using boost::asio::ip::tcp;

double get_percentile_latency_lca1(std::string host, int port) {
  double service_time = 0;
  try {
    // socket creation
    boost::system::error_code error;
    boost::asio::io_service io_service;
    tcp::socket socket(io_service);

    // connection
    // std::cout << "[Client] Connecting to server...: ";
    socket.connect(
        tcp::endpoint(boost::asio::ip::address::from_string(host), port),
        error);

    boost::array<char, 128> buf;

    size_t len = socket.read_some(boost::asio::buffer(buf), error);

    if (error == boost::asio::error::eof) {
      std::cout << "Connection closed cleanly by peer" << std::endl;
      exit(EXIT_FAILURE);  // Connection closed cleanly by peer.
    } else if (error)
      throw boost::system::system_error(error);  // Some other error.

    // std::cout.write(buf.data(), len);
    // std::cout << std::endl;
    std::string my_string(buf.begin(), len);
    service_time = boost::lexical_cast<double>(my_string);

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  return service_time;
}

double get_percentile_latency_lca2(std::string host, int port) {
  double service_time = 0;
  try {
    // socket creation
    boost::system::error_code error;
    boost::asio::io_service io_service;
    tcp::socket socket(io_service);

    // connection
    // std::cout << "[Client] Connecting to server...: ";
    socket.connect(
        tcp::endpoint(boost::asio::ip::address::from_string(host), port),
        error);

    boost::array<char, 128> buf;

    size_t len = socket.read_some(boost::asio::buffer(buf), error);

    if (error == boost::asio::error::eof) {
      std::cout << "Connection closed cleanly by peer" << std::endl;
      exit(EXIT_FAILURE);  // Connection closed cleanly by peer.
    } else if (error)
      throw boost::system::system_error(error);  // Some other error.

    // std::cout.write(buf.data(), len);
    // std::cout << std::endl;
    std::string my_string(buf.begin(), len);
    service_time = boost::lexical_cast<double>(my_string);
    service_time = std::ceil(service_time / 1e6 * 100) / 100;

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  return service_time;
}

unsigned long time_diff(struct timeval* start, struct timeval* stop) {
  unsigned long sec_res = stop->tv_sec - start->tv_sec;
  unsigned long usec_res = stop->tv_usec - start->tv_usec;
  return 1000000 * sec_res + usec_res;
}

int main(int argc, char* argv[]) {
  std::string host;
  int port, port2;

  struct timeval tstart, tend;
  unsigned long length;

  if (argc != 4) {
    std::cerr << "Usage: ./poll_n_clients <host> <port> <port2>" << std::endl;
    std::cerr << "Usage: ./poll_n_clients 146.193.41.56 1234 1235" << std::endl;
    return 1;
  }

  host = argv[1];
  port = std::stoi(argv[2]);
  port2 = std::stoi(argv[3]);
  double current_latency[2] = {0, 0};
  int i = 0;

  while (true) {
    // gettimeofday(&tstart, NULL);
    current_latency[0] = get_percentile_latency_lca1(host, port);
    current_latency[1] = get_percentile_latency_lca2(host, port2);
    if (current_latency[0] > 33 || current_latency[1] > 0) {
      /*if (current_latency > 1100){
              std::cout << i << "\t" << current_latency << std::endl;
              std::cout << "violation encountered" << std::endl;
             break;
      }*/
      std::cout << i << " lca1: " << current_latency[0]
                << "\tlca2: " << current_latency[1] << std::endl;
      // gettimeofday(&tend, NULL);
      // length = time_diff(&tstart, &tend);
      // std::cout << "This call took: " << (length) << " us" << std::endl;
      usleep(20000);
      // sleep(1);
      i++;
    }
  }

  return 0;
}
