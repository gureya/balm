/*
 * server.cpp
 *
 *  Created on: Sep 12, 2019
 *      Author: David Daharewa Gureya
 */

#include <boost/asio.hpp>
#include <iostream>
#include <vector>

using namespace boost::asio;
using ip::tcp;

struct CMYType {
  float a, b, c;
};

static_assert(std::is_pod<CMYType>::value, "Not bitwise serializable");

static std::ostream& operator<<(std::ostream& os, CMYType const& cmy) {
  return os << "[" << cmy.a << "," << cmy.b << "," << cmy.c << "]";
}

void server() {
  try {
    boost::system::error_code ec;
    boost::asio::io_service io_service;

    ip::tcp::acceptor acc(io_service, ip::tcp::endpoint { { }, 6767 });

    ip::tcp::socket sock(io_service);
    std::cout << "[Server] Waiting for connection...\n";

    acc.accept(sock, ec);
    std::cout << "[Server] Accepted a connection from client\n";

    std::vector<CMYType> data(10);
    std::size_t length = read(sock, buffer(data), ec);
    if (ec == boost::asio::error::eof) {
      std::cout << "Connection closed cleanly by peer\n";
    } else if (ec) {
      throw boost::system::system_error(ec);  // Some other error
    }

    std::cout << "length:" << length << " data: { ";

    for (auto& cmy : data)
      std::cout << cmy << ", ";

    std::cout << " }\n";

   // for (int i = 0; i < data.size(); i++) {
   //   std::cout << data.at(i).a << std::endl;
   // }

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

}

int main() {
  server();
  return 0;
}
