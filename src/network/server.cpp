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
                                    close();
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
                                            nlohmann::json::parse(std::string(data_, bytes_transferred)),
                                            get(),
                                            getServer().database_manager_,
                                            getServer().active_sessions_,
                                            getServer().active_users_);
                                    bool write_in_progress = !pending_responses_.empty();
                                    pending_responses_.insert(pending_responses_.end(), new_resps.begin(),
                                                              new_resps.end());
                                    if (!write_in_progress) {
                                        doWrite();
                                    }
                                    LOG(user_id_, " Success read and responses on request");
                                } else {
                                    close();
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
                                     close();
                                     LOG(user_id_, " ERROR on write: ", ec.message());
                                 }
                             });
}

void network::Session::close() {
    if (user_id_ != "John Doe" && getServer().active_users_.count(user_id_)) {
        getServer().active_sessions_.erase(getServer().active_users_.at(user_id_));
        getServer().active_users_.erase(user_id_);
    }
    socket_.lowest_layer().close();
}

network::response_queue
network::execRequest(const nlohmann::json &json, std::shared_ptr<network::ISession> owner_session,
                     std::shared_ptr<core::DataBaseManager> database_manager,
                     std::unordered_map<int, std::weak_ptr<network::ISession>> &active_sessions,
                     std::unordered_map<std::string, int> &active_users) {
    if (json.contains("user_id") && active_users.count(json["user_id"])) {
        LOG("get request from ", active_users.at(json["user_id"]));
    }
    response_queue new_responses;

    switch (json["request_type"].get<int>()) {
        case core::RequestAction::SELL_USD_REQUEST: {
            LOG("request type is sell usd");
            if (!active_users.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto res_id = database_manager->
                    MakeTransaction(
                    "INSERT INTO requests (user_id, type_of_operation, usd_count, rub_price, active) values(" +
                    std::to_string(active_users.at(json["user_id"])) + ", \'sell\'" + ", " +
                    std::to_string(json["usd_count"].get<int>()) + ", " +
                    std::to_string(json["rub_price"].get<int>()) + ", true) RETURNING id");
            database_manager->CommitTransactions();
            new_responses.push_back(std::make_shared<core::SuccessReqResponse>(std::atoi(res_id[0][0].c_str())));

            int usd_selling = 0;
            while (true) {
                if (usd_selling == json["usd_count"]) {
                    break;
                }

                auto matching_requests = database_manager->
                        MakeTransaction("SELECT * FROM requests "
                                        "WHERE rub_price >= " + std::to_string(json["rub_price"].get<int>()) +
                                        " AND type_of_operation = \'buy\' AND active = true "
                                        "ORDER BY rub_price DESC LIMIT 100");
                if (matching_requests.empty())
                    break;

                for (size_t i = 0; i < matching_requests.size() && usd_selling != json["usd_count"].get<int>(); i++) {
                    network::Server::TransactionInfo trans_info;
                    if (json["usd_count"].get<int>() > matching_requests[0][3].as<int>()) {
                        usd_selling += matching_requests[0][3].as<int>();

                        trans_info = closeRequest(active_users.at(json["user_id"]),
                                                                             matching_requests[0][1].as<int>(), usd_selling, false, database_manager);
                    } else if (json["usd_count"].get<int>() < matching_requests[0][3].as<int>()) {
                        usd_selling = json["usd_count"].get<int>();

                        trans_info = closeRequest(active_users.at(json["user_id"]),
                                                                             matching_requests[0][1].as<int>(), usd_selling, true, database_manager);
                    } else {
                        usd_selling = json["usd_count"].get<int>();

                        trans_info = closeTwoWayRequests(active_users.at(json["user_id"]),
                                                                                    matching_requests[0][1].as<int>(), usd_selling, database_manager);
                    }
                    if (active_sessions.count(matching_requests[0][1].as<int>())) {
                        auto session_ptr = active_sessions.at(matching_requests[0][1].as<int>());
                        if (!session_ptr.expired()) {
                            session_ptr.lock()->addResponsesQueue(std::make_shared<core::Report>(
                                    trans_info.id, trans_info.usd_count, trans_info.rub_price, trans_info.seller_name,
                                    trans_info.buyer_name));
                        }
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
            if (!active_users.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto res_id = database_manager->
                    MakeTransaction(
                    "INSERT INTO requests (user_id, type_of_operation, usd_count, rub_price, active) values(" +
                    std::to_string(active_users.at(json["user_id"])) + ", \'buy\'" + ", " +
                    std::to_string(json["usd_count"].get<int>()) + ", " +
                    std::to_string(json["rub_price"].get<int>()) + ", true) RETURNING id");
            database_manager->CommitTransactions();
            new_responses.push_back(std::make_shared<core::SuccessReqResponse>(std::atoi(res_id[0][0].c_str())));

            int usd_buying = 0;
            while (true) {
                if (usd_buying == json["usd_count"]) {
                    break;
                }

                auto matching_requests = database_manager->
                        MakeTransaction("SELECT * FROM requests "
                                        "WHERE rub_price <= " + std::to_string(json["rub_price"].get<int>()) +
                                        " AND type_of_operation = \'sell\' AND active = true "
                                        "ORDER BY rub_price DESC LIMIT 100");
                if (matching_requests.empty())
                    break;

                for (size_t i = 0; i < matching_requests.size() && usd_buying != json["usd_count"].get<int>(); i++) {
                    network::Server::TransactionInfo trans_info;
                    if (json["usd_count"].get<int>() > matching_requests[0][3].as<int>()) {
                        usd_buying += matching_requests[0][3].as<int>();

                        trans_info = closeRequest(matching_requests[0][1].as<int>(),
                                                                             active_users.at(json["user_id"]), usd_buying,
                                                                             true, database_manager);
                    } else if (json["usd_count"].get<int>() < matching_requests[0][3].as<int>()) {
                        usd_buying = json["usd_count"].get<int>();

                        trans_info = closeRequest(matching_requests[0][1].as<int>(),
                                                                             active_users.at(json["user_id"]), usd_buying,
                                                                             false, database_manager);
                    } else {
                        usd_buying = json["usd_count"].get<int>();

                        trans_info = closeTwoWayRequests(matching_requests[0][1].as<int>(),
                                                                                    active_users.at(json["user_id"]),
                                                                                    usd_buying, database_manager);
                    }
                    if (active_sessions.count(matching_requests[0][1].as<int>())) {
                        auto session_ptr = active_sessions.at(matching_requests[0][1].as<int>());
                        if (!session_ptr.expired()) {
                            session_ptr.lock()->addResponsesQueue(std::make_shared<core::Report>(
                                    trans_info.id, trans_info.usd_count, trans_info.rub_price, trans_info.seller_name,
                                    trans_info.buyer_name));
                        }
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
            if (!active_users.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto usd = database_manager->
                    MakeTransaction("SELECT usd_count FROM data WHERE id=" +
                                    std::to_string(active_users.at(json["user_id"])));
            auto rub = database_manager->
                    MakeTransaction("SELECT rub_balance FROM data WHERE id=" +
                                    std::to_string(active_users.at(json["user_id"])));
            new_responses.push_back(std::make_shared<core::BalanceResponse>(std::atoi(rub[0][0].c_str()),
                                                                            std::atoi(usd[0][0].c_str())));
            break;
        }
        case core::RequestAction::CANCEL_REQUEST: {
            LOG("request type is cancel request");
            if (!active_users.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto request_owner = database_manager->
                    MakeTransaction("SELECT user_id FROM requests WHERE id=" +
                                    std::to_string(json["request_id"].get<int>()));
            if (!strcmp(request_owner[0][0].c_str(),
                        std::to_string(active_users.count(json["user_id"])).c_str())) {
                auto is_active = database_manager->
                        MakeTransaction("SELECT active FROM requests WHERE id=" +
                                        std::to_string(json["request_id"].get<int>()));
                if (is_active[0][0].as<bool>()) {
                    database_manager->MakeTransaction(
                            "DELETE FROM requests WHERE id=" + std::to_string(json["request_id"].get<int>())
                    );
                    database_manager->CommitTransactions();
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
            auto name = database_manager->
                    MakeTransaction("SELECT id FROM data WHERE user_name=\'" +
                                    unquoted(json["name"].dump()) + "\'");
            if (name.empty()) {
                database_manager->
                        MakeTransaction("INSERT INTO data (user_name, password, usd_count, rub_balance) values(\'" +
                                        unquoted(json["name"].dump()) + "\',\'" +
                                        GetSHA1(unquoted(json["password"].dump())) +
                                        "\', 0, 0)");
                database_manager->CommitTransactions();
            } else {
                return {std::make_shared<core::BadResponse>("name occupied")};
            }
        }
        case core::RequestAction::AUTHORIZATION_REQUEST: {
            LOG("request type is authorization");
            auto user_data = database_manager->
                    MakeTransaction("SELECT id, password FROM data WHERE user_name=\'" +
                                    unquoted(json["name"].dump()) + "\'");
            if (user_data.empty() || strcmp(user_data[0][1].c_str(), GetSHA1(unquoted(json["password"].dump())).c_str()) != 0) {
                return {std::make_shared<core::BadResponse>("wrong password or name")};
            }

            int user_table_id = user_data[0][0].as<int>();
            std::string next_user_id;
            do {
                next_user_id = GetSHA1(std::to_string(random_generator::Random().GetNumber<int>()));
            } while (active_users.count(next_user_id));
            active_users.emplace(next_user_id, user_table_id);
            active_sessions.emplace(user_table_id, owner_session);
            owner_session->getUserId() = next_user_id;
            new_responses.push_back(std::make_shared<core::AccessAuthResponse>(next_user_id));

            break;
        }
        case core::RequestAction::UPDATE_REQUEST: {
            LOG("request type is update quotation");
            if (!active_users.count(json["user_id"]))
                return {std::make_shared<core::BadResponse>("not authorized")};

            auto quotation = database_manager->MakeTransaction(
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

            auto requests = database_manager->MakeTransaction(
                    "SELECT * FROM requests WHERE user_id = " +
                    std::to_string(active_users.at(json["user_id"])));
            std::vector<core::UpdateResponse::RequestInfo> reqs;
            for (auto req: requests) {
                reqs.emplace_back(
                        core::UpdateResponse::RequestInfo{req[0].as<int>(), req[1].as<int>(), req[2].as<std::string>(),
                                                          req[3].as<int>(), req[4].as<int>(), req[5].as<bool>()});
            }

            new_responses.push_back(std::make_shared<core::UpdateResponse>(std::move(usd_by_rub), std::move(reqs)));
            break;
        }
        default:
            LOG("wrong request type, should send bad response!");
            new_responses.push_back(std::make_shared<core::BadResponse>("test response"));
            break;
    }
    return new_responses;
}

network::Server::TransactionInfo network::closeRequest(int seller_id, int buyer_id, int usd_buying, bool is_sellout,
                                                       std::shared_ptr<core::DataBaseManager> database_manager) {
    auto seller_name = database_manager->
            MakeTransaction("SELECT user_name FROM data WHERE id = " +
                            std::to_string(
                                    seller_id))[0][0].c_str();

    auto buyer_name = database_manager->
            MakeTransaction("SELECT user_name FROM data WHERE id = " +
                            std::to_string(buyer_id))[0][0].c_str();

    pqxx::row row = database_manager->
            MakeTransaction1("SELECT * FROM requests "
                             "WHERE user_id=" + std::to_string(is_sellout ? seller_id : buyer_id));
    auto[id, user_id, type_of_operation, usd_count, rub_price, is_active] = row.as<int, int, std::string,
            int, int, bool>();

    database_manager->
            MakeTransaction(
            "UPDATE requests SET usd_count = usd_count - " + std::to_string(usd_buying) +
            " WHERE user_id = " +
            std::to_string(is_sellout ? buyer_id : seller_id));

    database_manager->
            MakeTransaction(std::string(
            "INSERT INTO deal_history (buyer_id, seller_id, request_id, date) values(" +
            std::to_string(buyer_id) + ", " +
            std::to_string(seller_id) + ", " +
            std::to_string(id) +
            ", NOW())"));

    database_manager->
            MakeTransaction(
            "UPDATE requests SET active = false WHERE user_id = " +
            std::to_string(user_id));

    database_manager->
            MakeTransaction(
            "UPDATE data SET usd_count = usd_count + " + std::to_string(usd_buying) +
            " WHERE id = " +
            std::to_string(buyer_id));

    database_manager->
            MakeTransaction(
            "UPDATE data SET rub_balance = rub_balance - \'" +
            std::to_string(rub_price * usd_buying) + "\' WHERE id = " +
            std::to_string(buyer_id));

    database_manager->
            MakeTransaction(
            "UPDATE data SET usd_count = usd_count - " + std::to_string(usd_buying) +
            " WHERE id = " +
            std::to_string(seller_id));

    database_manager->
            MakeTransaction(
            "UPDATE data SET rub_balance = rub_balance + " +
            std::to_string(usd_buying * rub_price) + " WHERE id = " +
            std::to_string(seller_id));

    database_manager->CommitTransactions();

    return {id, usd_count, rub_price, seller_name, buyer_name};
}

network::Server::TransactionInfo network::closeTwoWayRequests(int seller_id, int buyer_id, int usd_buying,
                                                              std::shared_ptr<core::DataBaseManager> database_manager) {
    auto seller_name = database_manager->
            MakeTransaction("SELECT user_name FROM data WHERE id = " +
                            std::to_string(
                                    seller_id))[0][0].c_str();

    auto buyer_name = database_manager->
            MakeTransaction("SELECT user_name FROM data WHERE id = " +
                            std::to_string(buyer_id))[0][0].c_str();

    pqxx::row row1 = database_manager->
            MakeTransaction1("SELECT * FROM requests "
                             "WHERE user_id=" + std::to_string(seller_id));
    auto[seller_req_id, seller_user_id, seller_type_of_operation, seller_usd_count, seller_rub_price, seller_is_active] = row1.as<int, int, std::string,
            int, int, bool>();

    pqxx::row row2 = database_manager->
            MakeTransaction1("SELECT * FROM requests "
                             "WHERE user_id=" + std::to_string(buyer_id));
    auto[buyer_req_id, buyer_user_id, buyer_type_of_operation, buyer_usd_count, buyer_rub_price, buyer_is_active] = row2.as<int, int, std::string,
            int, int, bool>();

    database_manager->
            MakeTransaction(std::string(
            "INSERT INTO deal_history (buyer_id, seller_id, request_id, date) values(" +
            std::to_string(buyer_id) + ", " +
            std::to_string(seller_id) + ", " +
            std::to_string(buyer_req_id) +
            ", NOW())"));

    database_manager->
            MakeTransaction(
            "UPDATE requests SET active = false WHERE user_id = " +
            std::to_string(buyer_user_id));

    database_manager->
            MakeTransaction(std::string(
            "INSERT INTO deal_history (buyer_id, seller_id, request_id, date) values(" +
            std::to_string(buyer_id) + ", " +
            std::to_string(seller_id) + ", " +
            std::to_string(seller_req_id) +
            ", NOW())"));

    database_manager->
            MakeTransaction(
            "UPDATE requests SET active = false WHERE user_id = " +
            std::to_string(seller_user_id));

    database_manager->
            MakeTransaction(
            "UPDATE data SET usd_count = usd_count + " + std::to_string(usd_buying) +
            " WHERE id = " +
            std::to_string(buyer_id));

    database_manager->
            MakeTransaction(
            "UPDATE data SET rub_balance = rub_balance - \'" +
            std::to_string(seller_rub_price * usd_buying) + "\' WHERE id = " +
            std::to_string(buyer_id));

    database_manager->
            MakeTransaction(
            "UPDATE data SET usd_count = usd_count - " + std::to_string(usd_buying) +
            " WHERE id = " +
            std::to_string(seller_id));

    database_manager->
            MakeTransaction(
            "UPDATE data SET rub_balance = rub_balance + " +
            std::to_string(usd_buying * seller_rub_price) + " WHERE id = " +
            std::to_string(seller_id));

    database_manager->CommitTransactions();

    return {buyer_req_id, buyer_usd_count, buyer_rub_price, seller_name, buyer_name};
}
