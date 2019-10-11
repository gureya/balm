/*
 * server.cpp
 *
 *  Created on: Sep 12, 2019
 *      Author: David Daharewa Gureya
 */

#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <iomanip>
#include <string>
#include <sstream>
#include <boost/serialization/vector.hpp>

using namespace boost::asio;
using ip::tcp;

enum {
  header_length = 8
};
//const header length

struct CMYType {
  float a, b, c;
  template<typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & a;
    ar & b;
    ar & c;
  }
};

struct network_data_in {
  char inbound_header_[header_length];  //size of data to read
  std::vector<char> inbound_data_;  // read data
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

    std::vector<CMYType> data;

    network_data_in data_in;
    size_t length;

    length = read(sock, buffer(data_in.inbound_header_), ec);
    //get size of data
    std::istringstream is(std::string(data_in.inbound_header_, header_length));
    std::size_t inbound_datasize = 0;
    is >> std::hex >> inbound_datasize;
    std::cout << " size in size_t: " << inbound_datasize << std::endl;

    data_in.inbound_data_.resize(inbound_datasize);  //resize the vector
    length = read(sock, buffer(data_in.inbound_data_), ec);

    //extract data
    std::string archive_data(&(data_in.inbound_data_[0]),
                             data_in.inbound_data_.size());
    std::istringstream archive_stream(archive_data);
    boost::archive::text_iarchive archive(archive_stream);
    archive >> data;  //deserialize

    std::cout << "length:" << data.size() << " data: { ";
    for (auto& cmy : data)
      std::cout << cmy << ", ";
    std::cout << " }\n";

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

}

int main() {
  server();
  return 0;
}
