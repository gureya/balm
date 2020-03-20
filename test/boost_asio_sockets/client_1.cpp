
#include <unistd.h>
#include <inttypes.h>

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

using namespace boost::asio;
using ip::tcp;
using std::cout;
using std::endl;
using std::string;

uint64_t client(std::string host, int port) {
  uint64_t service_us;

  try {
    boost::system::error_code error;
    boost::asio::io_service io_service;

    // socket creation
    tcp::socket socket(io_service);

    // connection
    std::cout << "[Client] Connecting to server..." << std::endl;
    socket.connect(
        tcp::endpoint(boost::asio::ip::address::from_string(host), port),
        error);

    if (!error) {
      // getting response from server
      boost::asio::streambuf receive_buffer;
      boost::asio::read(socket, receive_buffer, boost::asio::transfer_all(),
                        error);
      if (error && error != boost::asio::error::eof) {
        cout << "receive failed: " << error.message() << endl;
        exit(EXIT_FAILURE);
      } else {
        const char* data =
            boost::asio::buffer_cast<const char*>(receive_buffer.data());
        cout << "[Client] Receiving message: " << data << endl;
        // service_us = std::strtoull(data);
        service_us = boost::lexical_cast<uint64_t>(data);
      }

    } else {
      std::cout << "[Client] Connection failed <" << host << ":" << port
                << ">: " << error.message() << std::endl;
      exit(EXIT_FAILURE);
    }

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  return service_us;
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

  cout << "Host: " << host << "\tPort: " << port << endl;

  while (true) {
    uint64_t service_us = client(host, port);
    printf("service_us: %" PRIu64 "\n", service_us);
    sleep(5);
  }
  return 0;
}