#include "server.h"

#include <random>
#include <utility>

#include "bourse/request.h"
#include "bourse/response.h"
#include "logger.h"

network::Server::Server(boost::asio::io_context &io_context, unsigned short port,
                        std::shared_ptr<core::DataBaseManager> db_manager) : io_context_(io_context),
                                                                             acceptor_(io_context,
                                                                                       boost::asio::ip::tcp::endpoint(
                                                                                               boost::asio::ip::tcp::v4(),
                                                                                               port)),
                                                                             context_(
                                                                                     boost::asio::ssl::context::sslv23),
                                                                             database_manager_(std::move(db_manager)) {
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
    auto new_session = std::make_shared<Session>(*this, io_context_, context_);
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
                                    socket_.lowest_layer().close();
                                    LOG(user_id_, " ERROR on handshake: ", ec.message());
                                }
                            });
}

void network::Session::doRead() {
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
                            [this, self = shared_from_this()](boost::system::error_code ec,
                                                              size_t bytes_transferred) {
                                if (!ec) {
                                    auto new_resps = execRequest(
                                            nlohmann::json::parse(std::string(data_, bytes_transferred)));
                                    bool write_in_progress = !pending_responses_.empty();
                                    pending_responses_.insert(pending_responses_.end(), new_resps.begin(), new_resps.end());
                                    // непотокобезопасно
                                    if (!write_in_progress)
                                    {
                                        doWrite();
                                    }
                                    LOG(user_id_, " Success read and responses on request");
                                } else {
                                    socket_.lowest_layer().close();
                                    LOG(user_id_, " ERROR on read: ", ec.message());
                                }
                            });
}

void network::Session::doWrite() {
    auto reply = pending_responses_.front()->getJson().dump();
    strcpy(resp_buffer_, reply.c_str());
    boost::asio::async_write(socket_,
                             boost::asio::buffer(resp_buffer_, reply.size()),
                             [this, self = shared_from_this()](boost::system::error_code ec, size_t bytes_transferred) {
                                 if (!ec) {
                                     pending_responses_.pop_front();
                                     if (!pending_responses_.empty())
                                     {
                                         doWrite();
                                     } else doRead();
                                 } else {
                                     socket_.lowest_layer().close();
                                     LOG(user_id_, " ERROR on write: ", ec.message());
                                 }
                             });
}


network::response_queue
network::Session::execRequest(const nlohmann::json &json) {
    if (json.contains("user_id")) {
        LOG("get request from ", json["user_id"]);
    }
    response_queue new_responses;
    switch (json["request_type"].get<int>()) {
        case core::RequestAction::SELL_USD_REQUEST:
            LOG("request type is sell usd");

            break;
        case core::RequestAction::BUY_USD_REQUEST:
            LOG("request type is buy usd");
            break;
        case core::RequestAction::GET_BALANCE_REQUEST:
            LOG("request type is get balance");
            break;
        case core::RequestAction::CANCEL_REQUEST:
            LOG("request type is cancel request");
            break;
        case core::RequestAction::REGISTER_REQUEST:
            LOG("request type is registration");

            break;
        case core::RequestAction::AUTHORIZATION_REQUEST: {
            LOG("request type is authorization");
            // TODO проверка пароля
            int user_table_id = 5; // TODO получать id из базы
            std::string next_user_id;
            do {
                next_user_id = GetSHA1(std::to_string(random_generator::Random().GetNumber<int>()));
            } while (!getServer().active_users_.count(next_user_id));
            getServer().active_users_.emplace(next_user_id, user_table_id);
            new_responses.push_back(std::make_shared<core::AccessAuthResponse>(next_user_id));
            break;
        }
        case core::RequestAction::UPDATE_QUOTATION_REQUEST:
            LOG("request type is update quotation");
            break;
        default:
            LOG("wrong request type, should send bad response!");
            new_responses.push_back(std::make_shared<core::BadResponse>("test response"));
            break;
    }
    return new_responses;
}
