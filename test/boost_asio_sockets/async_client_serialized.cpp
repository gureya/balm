/*
 * async_client_serialized.cpp
 *
 *  Created on: Sep 20, 2019
 *      Author: David Daharewa Gureya
 */

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <vector>
#include "connection_serialization.hpp" // Must come before boost/serialization headers.
#include <boost/serialization/vector.hpp>
#include "stock.hpp"

//namespace s11n_example {

/// Downloads stock quote information from a server.
class Client {
 public:
  /// Constructor starts the asynchronous connect operation.
  Client(boost::asio::io_service& io_service, const std::string& host,
         const std::string& service)
      : connection_(io_service) {
    // Resolve the host name into an IP address.
    boost::asio::ip::tcp::resolver resolver(io_service);
    boost::asio::ip::tcp::resolver::query query(host, service);
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver
        .resolve(query);

    // Start an asynchronous connect operation.
    boost::asio::async_connect(
        connection_.socket(),
        endpoint_iterator,
        boost::bind(&Client::handle_connect, this,
                    boost::asio::placeholders::error));
  }

  /// Handle completion of a connect operation.
  void handle_connect(const boost::system::error_code& e) {
    if (!e) {

      // Create the data to be sent to the server.
      stock s;
      s.code = "ABC";
      s.name = "A Big Company";
      s.open_price = 4.56;
      s.high_price = 5.12;
      s.low_price = 4.33;
      s.last_price = 4.98;
      s.buy_price = 4.96;
      s.buy_quantity = 1000;
      s.sell_price = 4.99;
      s.sell_quantity = 2000;
      stocks_.push_back(s);
      s.code = "DEF";
      s.name = "Developer Entertainment Firm";
      s.open_price = 20.24;
      s.high_price = 22.88;
      s.low_price = 19.50;
      s.last_price = 19.76;
      s.buy_price = 19.72;
      s.buy_quantity = 34000;
      s.sell_price = 19.85;
      s.sell_quantity = 45000;
      stocks_.push_back(s);

      // Successfully accepted a new connection. Send the list of stocks to the
      // client. The connection::async_write() function will automatically
      // serialize the data structure for us.
      connection_.async_write(
          stocks_,
          boost::bind(&Client::handle_write, this,
                      boost::asio::placeholders::error));
    } else {
      // An error occurred. Log it and return. Since we are not starting a new
      // operation the io_service will run out of work to do and the client will
      // exit.
      std::cerr << "An Erro has occured: " << e.message() << std::endl;
    }
  }

  /// Handle completion of a write operation.
  void handle_write(const boost::system::error_code& e) {
    // Nothing to do. The socket will be closed automatically when the last
    // reference to the connection object goes away.
  }

 private:
  /// The connection to the server.
  connection connection_;

  /// The data received from the server.
  std::vector<stock> stocks_;
};

//}  // namespace s11n_example

int main(int argc, char* argv[]) {
  try {
    // Check command line arguments.
    if (argc != 3) {
      std::cerr << "Usage: client <host> <port>" << std::endl;
      return 1;
    }

    boost::asio::io_service io_service;
    Client client(io_service, argv[1], argv[2]);
    io_service.run();
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

