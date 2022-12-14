#include "request.h"

#include <iostream>

// TODO мб стоит использовать boost::hana и сделать это всё с помощью рефлексии в одной функции getJson, которую будут просто наследовать!!

nlohmann::json core::CreateLotRequest::getJson() const {
    nlohmann::json json;
    json["request_type"] = operation_ == OperationType::SELL_USD ? SELL_USD_REQUEST : BUY_USD_REQUEST;
    json["user_id"] = user_id_;
    json["usd_count"] = usd_count_;
    json["rub_price"] = rub_price_;
    return std::move(json);
}

nlohmann::json core::BalanceRequest::getJson() const {
    nlohmann::json json;
    json["request_type"] = GET_BALANCE_REQUEST;
    json["user_id"] = user_id_;
    return std::move(json);
}

nlohmann::json core::CancelRequest::getJson() const {
    nlohmann::json json;
    json["request_type"] = CANCEL_REQUEST;
    json["user_id"] = user_id_;
    json["request_id"] = request_id_;
    return std::move(json);
}

nlohmann::json core::AuthorizationRequest::getJson() const {
    nlohmann::json json;
    json["request_type"] = AUTHORIZATION_REQUEST;
    json["name"] = name_;
    json["password"] = password_;
    return std::move(json);
}

nlohmann::json core::RegistrationRequest::getJson() const {
    nlohmann::json json;
    json["request_type"] = REGISTER_REQUEST;
    json["name"] = name_;
    json["password"] = password_;
    return std::move(json);
}

nlohmann::json core::UpdateRequest::getJson() const {
    nlohmann::json json;
    json["request_type"] = UPDATE_REQUEST;
    json["user_id"] = user_id_;
    return std::move(json);
}

std::shared_ptr<core::IRequest> core::makeRequest(const std::string &msg) {
    if (msg == "SELL") {
        int usd_count;
        int rub_price;
        std::cout << "input usd u want to sell and rub price" << std::endl;
        std::cin >> usd_count >> rub_price;
        return std::make_shared<core::CreateLotRequest>("qweqe", usd_count, rub_price,
                                                        core::CreateLotRequest::OperationType::SELL_USD);
    } else if (msg == "REG") {
        std::string name;
        std::string password = "0";
        std::cout << "enter your login: " << std::flush;
        std::cin >> name;
        std::string repeat_password = "1";
        bool first = true;
        do {
            if (!first) {
                std::cout << "The passwords don't match" << std::endl;
            }
            first = false;
            std::cout << "enter your password:  " << std::flush;;
            std::cin >> password;
            std::cout << "repeat your password: ";
            std::cin >> repeat_password;
        } while (password != repeat_password);
        return std::make_shared<core::RegistrationRequest>(name, password);
    }
    return nullptr;
}