#include "server.h"

#include "bourse/request.h"
#include "bourse/response.h"
#include "logger.h"

network::Server::Server(boost::asio::io_context &io_context, unsigned short port) : io_context_(io_context),
                                                                                    acceptor_(io_context,
                                                                                              boost::asio::ip::tcp::endpoint(
                                                                                                      boost::asio::ip::tcp::v4(),
                                                                                                      port)),
                                                                                    context_(
                                                                                            boost::asio::ssl::context::sslv23),
                                                                                            port_(port){
    context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
    context_.use_certificate_chain_file("server_certificate.crt");
    context_.use_private_key_file("server.key", boost::asio::ssl::context::pem);
    context_.use_tmp_dh_file("dh2048.pem");

    doAccept();
}

void network::Server::doAccept() {
    auto new_session = std::make_shared<Session>(io_context_, context_);
    acceptor_.async_accept(new_session->socket(),
                           [this, new_session](boost::system::error_code ec) {
                               if (!ec) {
                                   new_session->start();
                               }
                               doAccept();
                           });
}

void network::Session::start() {
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
                            [this, self = shared_from_this()](boost::system::error_code ec) {
                                if (!ec) {
                                    doRead();
                                    LOG(user_id_, " Success handshake");
                                } else {
                                    LOG(user_id_, " ERROR on handshake: ", ec.message());
                                }
                            });
}

void network::Session::doRead() {
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
                            [this, self=shared_from_this()](boost::system::error_code ec,
                                         size_t bytes_transferred){
                                if (!ec){
                                    std::shared_ptr<core::IResponse> resp = network::execRequest(nlohmann::json::parse(std::string(data_, bytes_transferred)), database.lock());
                                    auto reply = resp->getJson().dump();
                                    doWrite(reply);
                                    LOG(user_id_, " Success read and responses on request");
                                } else {
                                    LOG(user_id_, " ERROR on read: ", ec.message());
                                }
                            });
}

void network::Session::doWrite(std::string const & resp) {
    boost::asio::async_write(socket_,
                             boost::asio::buffer(resp.data(), resp.size()),
                             [this, self=shared_from_this()](boost::system::error_code ec, size_t bytes_transferred){
        if (!ec) {
            doRead();
            LOG(user_id_, " Success write: ", ec.message());
        } else {
            LOG(user_id_, " ERROR on write: ", ec.message());
        }
    });
}


core::ResponseType network::execRequest(const nlohmann::json &json, const std::shared_ptr<core::DataBaseManager>& db_manager) {
    std::cout << json["request_type"];
    return nullptr;
}
