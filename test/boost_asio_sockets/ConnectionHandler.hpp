/*
 * ConnectionHandler.hpp
 *
 *  Created on: Sep 12, 2019
 *      Author: David Daharewa Gureya
 */

#ifndef TEST_CONNECTIONHANDLER_HPP_
#define TEST_CONNECTIONHANDLER_HPP_

//importing libraries
#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <vector>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <string>
#include <sstream>
#include <cstdint>
#include <boost/serialization/vector.hpp>

using namespace boost::asio;
using ip::tcp;

struct CMYType {
  uintptr_t pageAlignedStartAddress;
  unsigned long pageAlignedLength;
  pid_t processID;
  template<typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & pageAlignedStartAddress;
    ar & pageAlignedLength;
    ar & processID;
  }
};

static std::ostream& operator<<(std::ostream& os, CMYType const& cmy) {
  return os << "[" << cmy.pageAlignedStartAddress << ","
            << cmy.pageAlignedLength << "," << cmy.processID << "]";
}

class con_handler : public boost::enable_shared_from_this<con_handler> {
 private:
  tcp::socket sock;
  std::vector<CMYType> data;

  /// The size of a fixed length header.
  enum {
    header_length = 8
  };

  struct network_data_in {
    char inbound_header_[header_length];  //size of data to read
    std::vector<char> inbound_data_;  // read data
  };
  network_data_in data_in;

 public:
  typedef boost::shared_ptr<con_handler> pointer;
  con_handler(boost::asio::io_service& io_service)
      : sock(io_service) {
  }
// creating the pointer
  static pointer create(boost::asio::io_service& io_service) {
    return pointer(new con_handler(io_service));
  }
//socket creation
  tcp::socket& socket() {
    return sock;
  }

  void start() {

    //first receive a string indicating the size of the vector!
    boost::asio::async_read(
        sock,
        boost::asio::buffer(data_in.inbound_header_),
        boost::bind(&con_handler::handle_read_size, shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }

  void handle_read_size(const boost::system::error_code& err,
                        size_t bytes_transferred) {
    if (!err) {
      //get size of data
      std::istringstream is(
          std::string(data_in.inbound_header_, header_length));
      std::size_t inbound_datasize = 0;
      is >> std::hex >> inbound_datasize;
      //std::cout << "size in size_t: " << inbound_datasize << std::endl;

      data_in.inbound_data_.resize(inbound_datasize);  //resize the vector

      //now read the data!
      boost::asio::async_read(
          sock,
          boost::asio::buffer(data_in.inbound_data_),
          boost::bind(&con_handler::handle_read_data, shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
    } else {
      std::cerr << "error: " << err.message() << std::endl;
      sock.close();
    }
  }

  void handle_read_data(const boost::system::error_code& err,
                        size_t bytes_transferred) {
    if (!err) {

      //extract data
      std::string archive_data(&(data_in.inbound_data_[0]),
                               data_in.inbound_data_.size());
      std::istringstream archive_stream(archive_data);
      boost::archive::text_iarchive archive(archive_stream);
      archive >> data;  //deserialize

      std::cout << "length: " << data.size() << " data: { ";

      for (auto& cmy : data)
        std::cout << cmy << ", ";

      std::cout << " }\n";
    } else {
      std::cerr << "error: " << err.message() << std::endl;
      sock.close();
    }
  }
};

class Server {
 private:
  tcp::acceptor acceptor_;
  void start_accept() {
    // socket
    con_handler::pointer connection = con_handler::create(
        acceptor_.get_io_service());

    // asynchronous accept operation and wait for a new connection.
    acceptor_.async_accept(
        connection->socket(),
        boost::bind(&Server::handle_accept, this, connection,
                    boost::asio::placeholders::error));
  }
 public:
//constructor for accepting connection from client
  Server(boost::asio::io_service& io_service)
      : acceptor_(io_service, tcp::endpoint(tcp::v4(), 6767)) {
    std::cout << "[Server] Waiting for connection...\n";
    start_accept();
  }
  void handle_accept(con_handler::pointer connection,
                     const boost::system::error_code& err) {
    if (!err) {
      connection->start();
      std::cout << "[Server] Accepted a connection from client\n";
    }
    start_accept();
  }
};

#endif /* TEST_CONNECTIONHANDLER_HPP_ */
