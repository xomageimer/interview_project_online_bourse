#ifndef BOURSE_SERVER_H
#define BOURSE_SERVER_H

#include <iostream>

#include "bourse/database_manager.h"
#include "auxiliary.h"

#include "nlohmann/json.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <deque>
#include <utility>

using boost::asio::ip::tcp;

namespace core{
    struct IResponse;
}

namespace network {

    using ssl_socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
    using response_queue = std::deque<std::shared_ptr<core::IResponse>>;

    enum {
        max_length = 8'192
    };

    struct Session : public std::enable_shared_from_this<Session> {
    public:
        Session(struct Server & server, boost::asio::io_context &io_context,
                boost::asio::ssl::context &context) : server_(server), socket_(io_context, context) {};

        ssl_socket::lowest_layer_type &socket() {
            return socket_.lowest_layer();
        }

        void start();

        void doRead();

        void doWrite();

        std::string const & getUserId() const { return user_id_; }

        Server & getServer() { return server_; }

    private:
        response_queue execRequest(nlohmann::json const & json);

        Server & server_;

        std::string user_id_ = "John Doe";
        ssl_socket socket_;

        char data_[max_length]{};

        response_queue pending_responses_;
        char resp_buffer_[max_length]{};
        friend class Server;
    };

    struct Server {
    public:
        Server(boost::asio::io_context &io_context, unsigned short port, std::shared_ptr<core::DataBaseManager> db_manager);

    private:
        void doAccept();

        struct TransactionInfo {
            int id;
            int usd_count;
            int rub_price;
            std::string seller_name;
            std::string buyer_name;
        };

        TransactionInfo closeRequest(int seller_id, int buyer_id, int usd_buying, bool is_sellout);
        TransactionInfo closeTwoWayRequests(int seller_id, int buyer_id, int usd_buying);

        boost::asio::io_context &io_context_;
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::asio::ssl::context context_;

        std::shared_ptr<core::DataBaseManager> database_manager_;

        std::unordered_map<int, std::weak_ptr<Session>> active_sessions_;
        std::unordered_map<std::string, int> active_users_;
        friend class Session;
    };
}

#endif //BOURSE_SERVER_H
