/*
 * client.cpp
 *
 *  Created on: Sep 12, 2019
 *      Author: David Daharewa Gureya
 *
 */

//compiling: g++-8 client.cpp -o client -lboost_system -lpthread
#include <iostream>
#include <boost/asio.hpp>

using namespace boost::asio;
using ip::tcp;

struct CMYType {
  float a, b, c;
};

void client() {

  try {

    boost::system::error_code ec;
    boost::asio::io_service io_service;

    ip::tcp::socket sock(io_service);
    std::cout << "[Client] Connecting to server..." << std::endl;
    sock.connect( { { }, 6767 }, ec);

    if (!ec) {
      std::cout << "[Client] Connection successful\n";

      std::vector<CMYType> data { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }, { 10,
          11, 12 }, { 13, 14, 15 }, { 16, 17, 18 }, { 19, 20, 21 },
          { 22, 23, 24 }, { 25, 26, 27 }, { 28, 29, 30 }, };
      std::size_t length = write(sock, buffer(data), ec);

      if (!ec) {
        std::cout << "[Client] Sending data successful\n";
        std::cout << "sent: " << length << " bytes\n";
      } else {
        std::cout << "[Client] Sending data failed: " << ec.message()
                  << std::endl;
      }

    } else {
      std::cout << "[Client] Connection failed: " << ec.message() << std::endl;
    }

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

int main() {

  client();

  return 0;
}
