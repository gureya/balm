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

using namespace boost::asio;
using ip::tcp;

struct CMYType {
  float a, b, c;
};

//std::vector<CMYType> data(10);
std::size_t length;

static std::ostream& operator<<(std::ostream& os, CMYType const& cmy) {
  return os << "[" << cmy.a << "," << cmy.b << "," << cmy.c << "]";
}

class con_handler : public boost::enable_shared_from_this<con_handler> {
 private:
  tcp::socket sock;
  std::vector<CMYType> data;

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
    //data.resize(10);
    boost::asio::async_read(
        sock,
        boost::asio::buffer(data),
        boost::bind(&con_handler::handle_read, shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }

  void handle_read(const boost::system::error_code& err,
                   size_t bytes_transferred) {
    if (!err) {
      std::cout << "length:" << bytes_transferred << " data: { ";

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
