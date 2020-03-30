//
// client.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <iostream>

using boost::asio::ip::tcp;

int client(std::string host, int port) {
  try {
    // socket creation
    boost::system::error_code error;
    boost::asio::io_service io_service;
    tcp::socket socket(io_service);

    // connection
    std::cout << "[Client] Connecting to server...: ";
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

    std::cout.write(buf.data(), len);
    std::cout << std::endl;

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  // return 0;
}

int main(int argc, char* argv[]) {
  std::string host;
  int port;

  if (argc != 3) {
    std::cerr << "Usage: client <host> <port>" << std::endl;
    return 1;
  }

  host = argv[1];
  port = std::stoi(argv[2]);

  while (true) {
    client(host, port);
    sleep(5);
  }

  return 0;
}
