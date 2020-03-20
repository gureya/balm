/*
 * client.cpp
 *
 *  Created on: Sep 12, 2019
 *      Author: David Daharewa Gureya
 *
 */

// compiling: g++-8 client.cpp -o client -lboost_system -lboost_serialization
// -lpthread
// credits:
// https://stackoverflow.com/questions/9336010/how-to-read-a-fix-sized-packet-using-boost-asio
// credits:
// https://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/examples.html

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/serialization/vector.hpp>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace boost::asio;
using ip::tcp;

enum { header_length = 8 };

struct CMYType {
  uintptr_t pageAlignedStartAddress;
  unsigned long pageAlignedLength;
  pid_t processID;
  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& pageAlignedStartAddress;
    ar& pageAlignedLength;
    ar& processID;
  }
};

struct network_data_out {
  std::string outbound_header_;  // size of data to read
  std::string outbound_data_;    // read data
};

static_assert(std::is_pod<CMYType>::value, "Not bitwise serializable");

static std::ostream& operator<<(std::ostream& os, CMYType const& cmy) {
  return os << "[" << cmy.pageAlignedStartAddress << ","
            << cmy.pageAlignedLength << "," << cmy.processID << "]";
}

void client() {
  try {
    boost::system::error_code ec;
    boost::asio::io_service io_service;

    ip::tcp::socket sock(io_service);
    std::cout << "[Client] Connecting to server..." << std::endl;
    sock.connect({{}, 6767}, ec);

    if (!ec) {
      std::cout << "[Client] Connection successful\n";

      int a = 20;
      uintptr_t i = reinterpret_cast<uintptr_t>(&a);
      std::vector<CMYType> data{
          {i, 2, 3},   {i, 5, 6},   {i, 8, 9},   {i, 11, 12}, {i, 14, 15},
          {i, 17, 18}, {i, 20, 21}, {i, 23, 24}, {i, 26, 27}, {i, 29, 30},
      };

      std::cout << "length:" << data.size() << " data: { ";
      for (auto& cmy : data) std::cout << cmy << ", ";
      std::cout << " }\n";

      network_data_out data_out;

      // Serialize the data first so we know how large it is.
      std::ostringstream archive_stream;
      boost::archive::text_oarchive archive(archive_stream);
      archive << data;
      data_out.outbound_data_ = archive_stream.str();

      // give header length
      std::ostringstream header_stream;
      header_stream << std::setw(
                           header_length)  // set a field padding for header
                    << std::hex            // set next val to hexadecimal
                    << data_out.outbound_data_.size();  // write size in hexa

      data_out.outbound_header_ =
          header_stream
              .str();  // m_outbound_head == size in hexa in a std::string

      // m_outbound_header = [ 8 byte size ]
      // m_outbound_data = [ serialized data ]

      // write all data in the std::vector and send it
      std::vector<boost::asio::const_buffer> buffer;
      buffer.push_back(boost::asio::buffer(data_out.outbound_header_));
      buffer.push_back(boost::asio::buffer(data_out.outbound_data_));
      std::size_t length = write(sock, buffer, ec);

      if (!ec) {
        std::cout << "[Client] Sending actual data successful\n";
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
  while (true) {
    client();
    sleep(5);
  }
  return 0;
}
