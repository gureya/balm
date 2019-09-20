/*
 * async_server_serialized.cpp
 *
 *  Created on: Sep 20, 2019
 *      Author: David Daharewa Gureya
 */

//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <vector>
#include "connection_serialization.hpp" // Must come before boost/serialization headers.
#include <boost/serialization/vector.hpp>
#include "stock.hpp"

//namespace s11n_example {

/// Serves stock quote information to any client that connects to it.
class Server {
 public:
  /// Constructor opens the acceptor and starts waiting for the first incoming
  /// connection.
  Server(boost::asio::io_service& io_service, unsigned short port)
      : acceptor_(
          io_service,
          boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {

    // Start an accept operation for a new connection.
    connection_ptr new_conn(new connection(acceptor_.get_io_service()));
    acceptor_.async_accept(
        new_conn->socket(),
        boost::bind(&Server::handle_accept, this,
                    boost::asio::placeholders::error, new_conn));
  }

  /// Handle completion of a accept operation.
  void handle_accept(const boost::system::error_code& e, connection_ptr conn) {

    if (!e) {
      // Successfully established connection. Start operation to read the list
      // of stocks. The connection::async_read() function will automatically
      // decode the data that is read from the underlying socket.
      conn->async_read(
          stocks_,
          boost::bind(&Server::handle_read, this,
                      boost::asio::placeholders::error));
    } else {
      // An error occurred. Log it and return. Since we are not starting a new
      // operation the io_service will run out of work to do and the client will
      // exit.
      std::cerr << e.message() << std::endl;
    }

    // Start an accept operation for a new connection.
   // connection_ptr new_conn(new connection(acceptor_.get_io_service()));
   // acceptor_.async_accept(
   //     new_conn->socket(),
   //     boost::bind(&Server::handle_accept, this,
   //                 boost::asio::placeholders::error, new_conn));
  }

  /// Handle completion of a read operation.
  void handle_read(const boost::system::error_code& e) {
    if (!e) {
      // Print out the data that was received.
      for (std::size_t i = 0; i < stocks_.size(); ++i) {
        std::cout << "Stock number " << i << "\n";
        std::cout << "  code: " << stocks_[i].code << "\n";
        std::cout << "  name: " << stocks_[i].name << "\n";
        std::cout << "  open_price: " << stocks_[i].open_price << "\n";
        std::cout << "  high_price: " << stocks_[i].high_price << "\n";
        std::cout << "  low_price: " << stocks_[i].low_price << "\n";
        std::cout << "  last_price: " << stocks_[i].last_price << "\n";
        std::cout << "  buy_price: " << stocks_[i].buy_price << "\n";
        std::cout << "  buy_quantity: " << stocks_[i].buy_quantity << "\n";
        std::cout << "  sell_price: " << stocks_[i].sell_price << "\n";
        std::cout << "  sell_quantity: " << stocks_[i].sell_quantity << "\n";
      }
    } else {
      // An error occurred.
      std::cerr << "An Error has occured: " << e.message() << std::endl;
    }
  }

 private:
/// The acceptor object used to accept incoming socket connections.
  boost::asio::ip::tcp::acceptor acceptor_;

/// The data to be sent to each client.
  std::vector<stock> stocks_;
};

//}  // namespace s11n_example

int main(int argc, char* argv[]) {
  try {
    // Check command line arguments.
    if (argc != 2) {
      std::cerr << "Usage: server <port>" << std::endl;
      return 1;
    }
    unsigned short port = boost::lexical_cast<unsigned short>(argv[1]);

    boost::asio::io_service io_service;
    Server server(io_service, port);
    io_service.run();
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

