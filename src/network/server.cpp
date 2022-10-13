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
                                    pending_responses_.insert(pending_responses_.end(), new_resps.begin(),
                                                              new_resps.end());
                                    // непотокобезопасно
                                    if (!write_in_progress) {
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
                                     if (!pending_responses_.empty()) {
                                         doWrite();
                                     } else doRead();
                                 } else {
                                     socket_.lowest_layer().close();
                                     LOG(user_id_, " ERROR on write: ", ec.message());
                                 }
                             });
}

network::Server::TransactionInfo
network::Server::closeRequest(int seller_id, int buyer_id, int usd_buying, bool is_sellout) {
    auto seller_name = database_manager_->
            MakeTransaction("SELECT user_name FROM data WHERE id = " +
                            std::to_string(
                                    seller_id))[0][0].c_str();

    auto buyer_name = database_manager_->
            MakeTransaction("SELECT user_name FROM data WHERE id = " +
                            std::to_string(buyer_id))[0][0].c_str();

    pqxx::row row = database_manager_->
            MakeTransaction1("SELECT * FROM requests "
                             "WHERE user_id=" + std::to_string(is_sellout ? seller_id : buyer_id));
    auto[id, user_id, type_of_operation, usd_count, rub_price, is_active] = row.as<int, int, std::string,
            int, int, bool>();

    database_manager_->
            MakeTransaction(
            "UPDATE requests SET usd_count = usd_count - " + std::to_string(usd_buying) +
            " WHERE user_id = " +
            std::to_string(is_sellout ? buyer_id : seller_id));

    database_manager_->
            MakeTransaction(std::string(
            "INSERT INTO deal_history (buyer_id, seller_id, request_id, date) values(" +
            std::to_string(buyer_id) + ", " +
            std::to_string(seller_id) + ", " +
            std::to_string(id) +
            ", NOW())"));

    database_manager_->
            MakeTransaction(
            "UPDATE requests SET active = false WHERE user_id = " +
            std::to_string(user_id));

    database_manager_->
            MakeTransaction(
            "UPDATE data SET usd_count = usd_count + " + std::to_string(usd_buying) +
            " WHERE user_id = " +
            std::to_string(buyer_id));

    database_manager_->
            MakeTransaction(
            "UPDATE data SET rub_balance = rub_balance - \'" +
            std::to_string(rub_price * usd_buying) + "\' WHERE user_id = " +
            std::to_string(buyer_id));

    database_manager_->
            MakeTransaction(
            "UPDATE data SET usd_count = usd_count - " + std::to_string(usd_buying) +
            " WHERE user_id = " +
            std::to_string(seller_id));

    database_manager_->
            MakeTransaction(
            "UPDATE data SET rub_balance = rub_balance + " +
            std::to_string(usd_buying * rub_price) + " WHERE user_id = " +
            std::to_string(seller_id));

    return {id, usd_count, rub_price, seller_name, buyer_name};
}

network::Server::TransactionInfo network::Server::closeTwoWayRequests(int seller_id, int buyer_id, int usd_buying) {
    auto seller_name = database_manager_->
            MakeTransaction("SELECT user_name FROM data WHERE id = " +
                            std::to_string(
                                    seller_id))[0][0].c_str();

    auto buyer_name = database_manager_->
            MakeTransaction("SELECT user_name FROM data WHERE id = " +
                            std::to_string(buyer_id))[0][0].c_str();

    pqxx::row row1 = database_manager_->
            MakeTransaction1("SELECT * FROM requests "
                             "WHERE user_id=" + std::to_string(seller_id));
    auto[seller_req_id, seller_user_id, seller_type_of_operation, seller_usd_count, seller_rub_price, seller_is_active] = row1.as<int, int, std::string,
            int, int, bool>();

    pqxx::row row2 = database_manager_->
            MakeTransaction1("SELECT * FROM requests "
                             "WHERE user_id=" + std::to_string(buyer_id));
    auto[buyer_req_id, buyer_user_id, buyer_type_of_operation, buyer_usd_count, buyer_rub_price, buyer_is_active] = row2.as<int, int, std::string,
            int, int, bool>();

    database_manager_->
            MakeTransaction(std::string(
            "INSERT INTO deal_history (buyer_id, seller_id, request_id, date) values(" +
            std::to_string(buyer_id) + ", " +
            std::to_string(seller_id) + ", " +
            std::to_string(buyer_req_id) +
            ", NOW())"));

    database_manager_->
            MakeTransaction(
            "UPDATE requests SET active = false WHERE user_id = " +
            std::to_string(buyer_user_id));

    database_manager_->
            MakeTransaction(std::string(
            "INSERT INTO deal_history (buyer_id, seller_id, request_id, date) values(" +
            std::to_string(buyer_id) + ", " +
            std::to_string(seller_id) + ", " +
            std::to_string(seller_req_id) +
            ", NOW())"));

    database_manager_->
            MakeTransaction(
            "UPDATE requests SET active = false WHERE user_id = " +
            std::to_string(seller_user_id));

    database_manager_->
            MakeTransaction(
            "UPDATE data SET usd_count = usd_count + " + std::to_string(usd_buying) +
            " WHERE user_id = " +
            std::to_string(buyer_id));

    database_manager_->
            MakeTransaction(
            "UPDATE data SET rub_balance = rub_balance - \'" +
            std::to_string(seller_rub_price * usd_buying) + "\' WHERE user_id = " +
            std::to_string(buyer_id));

    database_manager_->
            MakeTransaction(
            "UPDATE data SET usd_count = usd_count - " + std::to_string(usd_buying) +
            " WHERE user_id = " +
            std::to_string(seller_id));

    database_manager_->
            MakeTransaction(
            "UPDATE data SET rub_balance = rub_balance + " +
            std::to_string(usd_buying * seller_rub_price) + " WHERE user_id = " +
            std::to_string(seller_id));

    return {buyer_req_id, buyer_usd_count, buyer_rub_price, seller_name, buyer_name};
}

network::response_queue
network::Session::execRequest(const nlohmann::json &json) {
    if (json.contains("user_id") && getServer().active_users_.count(json["user_id"])) {
        LOG("get request from ", getServer().active_users_.at(json["user_id"]));
    }
    response_queue new_responses;

    switch (json["request_type"].get<int>()) {
        case core::RequestAction::SELL_USD_REQUEST: {
            LOG("request type is sell usd");
            if (getServer().active_users_.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto res_id = getServer().database_manager_->
                    MakeTransaction(
                    "INSERT INTO requests (user_id, type_of_operation, usd_count, rub_price) values(" +
                    std::to_string(getServer().active_users_.at(json["user_id"])) + ", \'sell\'" + ", " +
                    std::to_string(json["usd_count"].get<int>()) + ", " +
                    std::to_string(json["rub_price"].get<int>()) + ") RETURNING id");
            getServer().database_manager_->CommitTransactions();
            new_responses.push_back(std::make_shared<core::SuccessReqResponse>(std::atoi(res_id[0][0].c_str())));

            int usd_selling = 0;
            while (true) {
                if (usd_selling == json["usd_count"]) {
                    break;
                }

                auto matching_requests = getServer().database_manager_->
                        MakeTransaction("SELECT * FROM requests "
                                        "WHERE rub_price >= " + std::to_string(json["rub_price"].get<int>()) +
                                        " AND active = true "
                                        "ORDER BY rub_price DESC LIMIT 100");
                if (matching_requests.empty())
                    break;

                for (size_t i = 0; i < matching_requests.size() && usd_selling != json["usd_count"].get<int>(); i++) {
                    Server::TransactionInfo trans_info;
                    if (json["usd_count"].get<int>() > matching_requests[0][3].as<int>()) {
                        usd_selling += matching_requests[0][3].as<int>();

                        trans_info = getServer().closeRequest(getServer().active_users_.at(json["user_id"]),
                                                              matching_requests[0][1].as<int>(), usd_selling, false);
                    } else if (json["usd_count"].get<int>() < matching_requests[0][3].as<int>()) {
                        usd_selling = json["usd_count"].get<int>();

                        trans_info = getServer().closeRequest(getServer().active_users_.at(json["user_id"]),
                                                              matching_requests[0][1].as<int>(), usd_selling, true);
                    } else {
                        usd_selling = json["usd_count"].get<int>();

                        trans_info = getServer().closeTwoWayRequests(getServer().active_users_.at(json["user_id"]),
                                                                     matching_requests[0][1].as<int>(), usd_selling);
                    }
                    if (getServer().active_sessions_.count(matching_requests[0][1].as<int>())) {
                        auto session_ptr = getServer().active_sessions_.at(matching_requests[0][1].as<int>()).lock();
                        session_ptr->pending_responses_.push_back(std::make_shared<core::Report>(
                                trans_info.id, trans_info.usd_count, trans_info.rub_price, trans_info.seller_name,
                                trans_info.buyer_name));
                    }
                    new_responses.push_back(std::make_shared<core::Report>(
                            trans_info.id, trans_info.usd_count, trans_info.rub_price, trans_info.seller_name,
                            trans_info.buyer_name));
                }
            }

            break;
        }
        case core::RequestAction::BUY_USD_REQUEST: {
            LOG("request type is buy usd");
            if (getServer().active_users_.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto res_id = getServer().database_manager_->
                    MakeTransaction(
                    "INSERT INTO requests (user_id, type_of_operation, usd_count, rub_price) values(" +
                    std::to_string(getServer().active_users_.at(json["user_id"])) + ", \'buy\'" + ", " +
                    std::to_string(json["usd_count"].get<int>()) + ", " +
                    std::to_string(json["rub_price"].get<int>()) + ") RETURNING id");
            getServer().database_manager_->CommitTransactions();
            new_responses.push_back(std::make_shared<core::SuccessReqResponse>(std::atoi(res_id[0][0].c_str())));

            int usd_buying = 0;
            while (true) {
                if (usd_buying == json["usd_count"]) {
                    break;
                }

                auto matching_requests = getServer().database_manager_->
                        MakeTransaction("SELECT * FROM requests "
                                        "WHERE rub_price <= " + std::to_string(json["rub_price"].get<int>()) +
                                        " AND active = true "
                                        "ORDER BY rub_price DESC LIMIT 100");
                if (matching_requests.empty())
                    break;

                for (size_t i = 0; i < matching_requests.size() && usd_buying != json["usd_count"].get<int>(); i++) {
                    Server::TransactionInfo trans_info;
                    if (json["usd_count"].get<int>() > matching_requests[0][3].as<int>()) {
                        usd_buying += matching_requests[0][3].as<int>();

                        trans_info = getServer().closeRequest(matching_requests[0][1].as<int>(),
                                                              getServer().active_users_.at(json["user_id"]), usd_buying,
                                                              false);
                    } else if (json["usd_count"].get<int>() < matching_requests[0][3].as<int>()) {
                        usd_buying = json["usd_count"].get<int>();

                        trans_info = getServer().closeRequest(matching_requests[0][1].as<int>(),
                                                              getServer().active_users_.at(json["user_id"]), usd_buying,
                                                              false);
                    } else {
                        usd_buying = json["usd_count"].get<int>();

                        trans_info = getServer().closeTwoWayRequests(matching_requests[0][1].as<int>(),
                                                                     getServer().active_users_.at(json["user_id"]),
                                                                     usd_buying);
                    }
                    if (getServer().active_sessions_.count(matching_requests[0][1].as<int>())) {
                        auto session_ptr = getServer().active_sessions_.at(matching_requests[0][1].as<int>()).lock();
                        session_ptr->pending_responses_.push_back(std::make_shared<core::Report>(
                                trans_info.id, trans_info.usd_count, trans_info.rub_price, trans_info.seller_name,
                                trans_info.buyer_name));
                    }
                    new_responses.push_back(std::make_shared<core::Report>(
                            trans_info.id, trans_info.usd_count, trans_info.rub_price, trans_info.seller_name,
                            trans_info.buyer_name));
                }
            }

            break;
        }
        case core::RequestAction::GET_BALANCE_REQUEST: {
            LOG("request type is get balance");
            if (getServer().active_users_.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto usd = getServer().database_manager_->
                    MakeTransaction("SELECT usd_count FROM data WHERE id=" +
                                    std::to_string(getServer().active_users_.at(json["user_id"])));
            auto rub = getServer().database_manager_->
                    MakeTransaction("SELECT rub_balance FROM data WHERE id=" +
                                    std::to_string(getServer().active_users_.at(json["user_id"])));
            new_responses.push_back(std::make_shared<core::BalanceResponse>(std::atoi(rub[0][0].c_str()),
                                                                            std::atoi(usd[0][0].c_str())));
            break;
        }
        case core::RequestAction::CANCEL_REQUEST: {
            LOG("request type is cancel request");
            if (getServer().active_users_.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto request_owner = getServer().database_manager_->
                    MakeTransaction("SELECT user_id FROM requests WHERE id=" +
                                    std::to_string(json["request_id"].get<int>()));
            if (!strcmp(request_owner[0][0].c_str(),
                        std::to_string(getServer().active_users_.count(json["user_id"])).c_str())) {
                auto is_active = getServer().database_manager_->
                        MakeTransaction("SELECT active FROM requests WHERE id=" +
                                        std::to_string(json["request_id"].get<int>()));
                if (!strcmp(is_active[0][0].c_str(), "t")) {
                    getServer().database_manager_->MakeTransaction(
                            "DELETE FROM requests WHERE id=" + std::to_string(json["request_id"].get<int>())
                    );
                    getServer().database_manager_->CommitTransactions();
                    new_responses.push_back(std::make_shared<core::SuccessReqResponse>(json["request_id"].get<int>()));
                } else {
                    new_responses.push_back(std::make_shared<core::BadResponse>("request has already been made"));
                }
            } else {
                new_responses.push_back(std::make_shared<core::BadResponse>("this request is not available to you"));
            }
            break;
        }
        case core::RequestAction::REGISTER_REQUEST: {
            LOG("request type is registration");
            auto name = getServer().database_manager_->
                    MakeTransaction("SELECT id FROM data WHERE user_name=" +
                                    (json["name"].dump()));
            if (name.empty()) {
                auto id = getServer().database_manager_->
                        MakeTransaction("INSERT INTO data (user_name, password, usd_count, rub_balance) values(\'" +
                                        json["name"].dump() + "\',\'" + GetSHA1(json["password"].dump()) +
                                        "\', 0, 0) RETURNING id");
                int user_table_id = id[0][0].as<int>();
                std::string next_user_id;
                do {
                    next_user_id = GetSHA1(std::to_string(random_generator::Random().GetNumber<int>()));
                } while (!getServer().active_users_.count(next_user_id));
                getServer().active_users_.emplace(next_user_id, user_table_id);
                getServer().active_sessions_.emplace(user_table_id, shared_from_this());
                new_responses.push_back(std::make_shared<core::AccessAuthResponse>(next_user_id));
            }
            break;
        }
        case core::RequestAction::AUTHORIZATION_REQUEST: {
            LOG("request type is authorization");
            auto user_data = getServer().database_manager_->
                    MakeTransaction("SELECT id, password FROM data WHERE user_name=" +
                                    (json["name"].dump()));
            if (user_data.empty() || strcmp(user_data[0][1].c_str(), GetSHA1(json["password"].dump()).c_str()) != 0) {
                return {std::make_shared<core::BadResponse>("wrong password or name")};
            }

            int user_table_id = user_data[0][0].as<int>();
            std::string next_user_id;
            do {
                next_user_id = GetSHA1(std::to_string(random_generator::Random().GetNumber<int>()));
            } while (!getServer().active_users_.count(next_user_id));
            getServer().active_users_.emplace(next_user_id, user_table_id);
            getServer().active_sessions_.emplace(user_table_id, shared_from_this());
            new_responses.push_back(std::make_shared<core::AccessAuthResponse>(next_user_id));
            break;
        }
        case core::RequestAction::UPDATE_QUOTATION_REQUEST: {
            LOG("request type is update quotation");
            if (getServer().active_users_.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto quotation = getServer().database_manager_->MakeTransaction(
                    "SELECT usd_count, rub_price FROM requests WHERE id = ANY (\n"
                    "    SELECT request_id FROM (\n"
                    "        SELECT * FROM deal_history ORDER BY date DESC LIMIT 2\n"
                    "    ) sub\n"
                    "    ORDER BY id ASC\n"
                    ")");
            std::vector<std::pair<int, int>> usd_by_rub;
            usd_by_rub.reserve(quotation.size());
            for (size_t i = 0; i < quotation.size(); i++) {
                usd_by_rub.emplace_back(std::pair<int, int>(quotation[i][0].as<int>(), quotation[i][1].as<int>()));
            }
            new_responses.push_back(std::make_shared<core::UpdateQuotationResponse>(std::move(usd_by_rub)));
            break;
        }
        default:
            LOG("wrong request type, should send bad response!");
            new_responses.push_back(std::make_shared<core::BadResponse>("test response"));
            break;
    }
    return new_responses;
}
