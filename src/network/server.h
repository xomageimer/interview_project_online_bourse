#ifndef BOURSE_SERVER_H
#define BOURSE_SERVER_H

#include <iostream>

#include "bourse/database_manager.h"

#include "nlohmann/json.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

using boost::asio::ip::tcp;

namespace core{
    struct IResponse;
}

namespace network {

    using ssl_socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
    enum {
        max_length = 2048
    };

    struct Session : public std::enable_shared_from_this<Session> {
    public:
        Session(boost::asio::io_context &io_context,
                boost::asio::ssl::context &context) : socket_(io_context, context) {};

        ssl_socket::lowest_layer_type &socket() {
            return socket_.lowest_layer();
        }

        void start();

        void doRead();

        void doWrite(std::string const & resp);

    private:
        std::string user_id_;
        ssl_socket socket_;
        char data_[max_length]{};

        std::weak_ptr<core::DataBaseManager> database;
    };

    struct Server {
    public:
        Server(boost::asio::io_context &io_context, unsigned short port);

    private:
        void doAccept();

        uint16_t port_{};
        boost::asio::io_context &io_context_;
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::asio::ssl::context context_;

        std::shared_ptr<core::DataBaseManager> database;
    };

    std::shared_ptr<struct core::IResponse> execRequest(nlohmann::json const & json, const std::shared_ptr<core::DataBaseManager>& db_manager);
}

#endif //BOURSE_SERVER_H
