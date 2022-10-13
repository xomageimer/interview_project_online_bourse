#include "response.h"

#include <iostream>

// TODO мб стоит использовать boost::hana и сделать это всё с помощью рефлексии в одной функции getJson, которую будут просто наследовать!!

nlohmann::json core::SuccessReqResponse::getJson() const {
    nlohmann::json json;
    json["response_type"] = SUCCESS_SET_RESPONSE;
    json["lot_id"] = lot_request_id_;
    return std::move(json);
}

nlohmann::json core::BalanceResponse::getJson() const {
    nlohmann::json json;
    json["response_type"] = GET_BALANCE_RESPONSE;
    json["rub_balance"] = rub_balance_;
    json["usd_count"] = usd_count_;
    return std::move(json);
}

nlohmann::json core::AccessAuthResponse::getJson() const {
    nlohmann::json json;
    json["response_type"] = AUTHORIZATION_RESPONSE;
    json["user_id"] = user_id_;
    return std::move(json);
}

nlohmann::json core::UpdateQuotationResponse::getJson() const {
    nlohmann::json json;
    json["response_type"] = UPDATE_QUOTATION_RESPONSE;
    json["quotation"] = usd_by_rub_;
    return std::move(json);
}

nlohmann::json core::BadResponse::getJson() const {
    nlohmann::json json;
    json["response_type"] = BAD_RESPONSE;
    json["message"] = message_;
    return std::move(json);
}

nlohmann::json core::Report::getJson() const {
    nlohmann::json json;
    json["response_type"] = REPORT;
    json["lot_id"] = lot_request_id_;
    json["usd_count"] = usd_count_;
    json["rub_price"] = rub_price_;
    json["seller_name"] = seller_name_;
    json["buyer_name"] = buyer_name_;
    return std::move(json);
}