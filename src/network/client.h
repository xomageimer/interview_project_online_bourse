#ifndef BOURSE_CLIENT_H
#define BOURSE_CLIENT_H

#include <string>
#include <optional>

#include "bourse/request.h"

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace network {

    using ssl_socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    enum {
        max_length = 2048
    };

    struct Client {
    public:
        Client(boost::asio::io_context& io_context,
               boost::asio::ssl::context& context,
               boost::asio::ip::tcp::resolver::results_type endpoints);

        bool verifyCertificate(bool preverified,
                               boost::asio::ssl::verify_context & ctx);

        void write(const core::RequestType& request);

        void close();

        void setUserId(std::string const & user_id);
    private:
        void doHandshake(const boost::system::error_code& error);

        void doWrite(std::string const & req);

        void doRead();

        ssl_socket socket_;
        std::optional<std::string> user_id_;
        char data_[max_length];
    };

    bool execResponse(const nlohmann::json &json, network::Client & my_client);
}

#endif //BOURSE_CLIENT_H
