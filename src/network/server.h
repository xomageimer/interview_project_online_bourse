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

namespace core {
    struct IResponse;
}

namespace network {

    using ssl_socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
    using response_queue = std::deque<std::shared_ptr<core::IResponse>>;

    enum {
        max_length = 8'192
    };

    struct ISession {
        virtual ~ISession() = default;

        virtual void addResponsesQueue(std::shared_ptr<core::IResponse>) = 0;

        std::string &getUserId() { return user_id_; }
    protected:
        std::string user_id_ = "John Doe";
    };

    struct Session : public ISession, public std::enable_shared_from_this<Session> {
    public:
        Session(struct Server &server, boost::asio::io_context &io_context,
                boost::asio::ssl::context &context) : server_(server), socket_(io_context, context) {};

        ssl_socket::lowest_layer_type &socket() {
            return socket_.lowest_layer();
        }

        auto get() {
            return shared_from_this();
        }

        void start();

        void doRead();

        void doWrite();

        Server &getServer() { return server_; }

        void addResponsesQueue(std::shared_ptr<core::IResponse> new_resp) override {
            bool write_in_progress = !pending_responses_.empty();
            pending_responses_.push_back(new_resp);
            if (!write_in_progress) {
                doWrite();
            }
        }

    private:
        void close();

        Server &server_;
        ssl_socket socket_;

        char data_[max_length]{};

        response_queue pending_responses_;
        char resp_buffer_[max_length]{};

        friend class Server;
    };

    struct Server {
    public:
        Server(boost::asio::io_context &io_context, unsigned short port,
               std::shared_ptr<core::DataBaseManager> db_manager);

        struct TransactionInfo {
            int id;
            int usd_count;
            int rub_price;
            std::string seller_name;
            std::string buyer_name;
        };
    private:
        void doAccept();

        boost::asio::io_context &io_context_;
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::asio::ssl::context context_;

        std::shared_ptr<core::DataBaseManager> database_manager_;

        std::unordered_map<int, std::weak_ptr<ISession>> active_sessions_;
        std::unordered_map<std::string, int> active_users_;

        friend class Session;
    };

    response_queue execRequest(nlohmann::json const &json,
                               std::shared_ptr<network::ISession> owner_session,
                               std::shared_ptr<core::DataBaseManager> database_manager,
                               std::unordered_map<int, std::weak_ptr<network::ISession>> & active_sessions,
                               std::unordered_map<std::string, int> & active_users);

    Server::TransactionInfo closeRequest(int request_id_completed, int request_id_not_completed, int usd_buying, int rub_price, std::shared_ptr<core::DataBaseManager> database_manager);

    Server::TransactionInfo closeTwoWayRequests(int request_id_seller, int request_id_buyer, int seller_id, int buyer_id, int usd_buying, int rub_price, std::shared_ptr<core::DataBaseManager> database_manager);
}

#endif //BOURSE_SERVER_H
