// importing libraries
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>

using namespace boost::asio;
using ip::tcp;
using std::cout;
using std::endl;

std::string message = "Hello From Server!";

uint64_t service_us = 1000;

std::string make_daytime_string() {
  using namespace std;  // For time_t, time and ctime;
  //time_t now = time(0);
  service_us += 1000;
  // return ctime(&now);
  return to_string(service_us);
}

class con_handler : public boost::enable_shared_from_this<con_handler> {
 private:
  tcp::socket sock;
  // std::string message = "Hello From Server!";
  std::string message_;
  enum { max_length = 1024 };
  char data[max_length];
  int i = 0;

 public:
  typedef boost::shared_ptr<con_handler> pointer;
  con_handler(boost::asio::io_service& io_service) : sock(io_service) {}
  // creating the pointer
  static pointer create(boost::asio::io_service& io_service) {
    return pointer(new con_handler(io_service));
  }
  // socket creation
  tcp::socket& socket() { return sock; }

  void start() {

    message_ = make_daytime_string();

    sock.async_write_some(
        boost::asio::buffer(message_, max_length),
        boost::bind(&con_handler::handle_write, shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }

  void handle_write(const boost::system::error_code& err,
                    size_t bytes_transferred) {
    if (!err) {
      cout << "[Server] Sending message: " << message_ << endl;
    } else {
      std::cerr << "error: " << err.message() << endl;
      sock.close();
    }
  }
};

class Server {
 private:
  tcp::acceptor acceptor_;
  void start_accept() {
    // socket
    con_handler::pointer connection =
        con_handler::create(acceptor_.get_io_service());

    // asynchronous accept operation and wait for a new connection.
    acceptor_.async_accept(connection->socket(),
                           boost::bind(&Server::handle_accept, this, connection,
                                       boost::asio::placeholders::error));
  }

 public:
  // constructor for accepting connection from client
  Server(boost::asio::io_service& io_service)
      : acceptor_(io_service, tcp::endpoint(tcp::v4(), 1234)) {
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