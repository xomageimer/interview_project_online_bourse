#include "client.h"

#include <utility>

#include "bourse/response.h"
#include "logger.h"

network::Client::Client(boost::asio::io_context &io_context, boost::asio::ssl::context &context,
                        boost::asio::ip::tcp::resolver::results_type endpoints) : socket_(io_context, context) {
    socket_.set_verify_mode(boost::asio::ssl::verify_peer);
    socket_.set_verify_callback(
            boost::bind(&Client::verifyCertificate, this, _1, _2));

    boost::asio::async_connect(socket_.lowest_layer(), endpoints,
                               boost::bind(&Client::doHandshake, this,
                                           boost::asio::placeholders::error));
}

bool network::Client::verifyCertificate(bool preverified, boost::asio::ssl::verify_context &ctx) {
    char subject_name[256];
    X509 *cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
    LOG("Verifying ", subject_name);

    return preverified;
}

void network::Client::doHandshake(const boost::system::error_code &error) {
    if (!error) {
        socket_.async_handshake(boost::asio::ssl::stream_base::client,
                                [this](boost::system::error_code ec) {
                                    if (!ec) {
                                        doRead();
                                    } else {
                                        LOG("Handshake failed: ", ec.message());
                                    }
                                });
    } else {
        LOG("Handshake failed: ", error.message());
    }
}

void network::Client::write(const core::RequestType& request) {
    doWrite(request->getJson().dump());
}

void network::Client::doWrite(std::string const &req) {
    boost::asio::async_write(socket_,
                             boost::asio::buffer(req.data(), req.size()),
                             [](boost::system::error_code ec,
                                size_t bytes_transferred) {
                                 if (ec) {
                                     LOG("Write failed: ", ec.message());
                                 }
                             });
}

void network::Client::doRead() {
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
                            [this](boost::system::error_code ec,
                                   size_t bytes_transferred) {
                                if (!ec) {
                                    auto json_msg = nlohmann::json::parse(std::string(data_, bytes_transferred));
                                    auto success = network::execResponse(json_msg,
                                                       *this);
                                    doRead();
                                } else {
                                    LOG("Write failed: ", ec.message());
                                }
                            });
}

void network::Client::setUserId(const std::string &user_id) {
    user_id_.emplace(user_id);
}

void network::Client::close() {
    boost::asio::post(socket_.get_executor(), [this]() { socket_.lowest_layer().close(); });
}

bool network::execResponse(const nlohmann::json &json, network::Client &my_client) {
    LOG("get to response by id ", json["response_type"]);
    std::cout << "get response " << json["response_type"] << std::endl;

    return true;
}
