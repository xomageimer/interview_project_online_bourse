#ifndef BOURSE_RESPONSE_H
#define BOURSE_RESPONSE_H

#include <memory>
#include <utility>
#include "nlohmann/json.hpp"
#include "request.h"


namespace network{
    struct Client;
}

namespace core {
    using ResponseType = std::shared_ptr<struct IResponse>;

    enum ResponseAction {
        SUCCESS_SET_RESPONSE = 0,
        BAD_RESPONSE,
        GET_BALANCE_RESPONSE,
        AUTHORIZATION_RESPONSE,
        UPDATE_RESPONSE,
        REPORT
    };

    // TODO надо как-то выполнять exec, но в том ли месте оно должно быть?

    struct IResponse {
        virtual ~IResponse() = default;

        [[nodiscard]] virtual nlohmann::json getJson() const = 0;
    };

    struct SuccessReqResponse : public IResponse {
    public:
        explicit SuccessReqResponse(int lot_request_id) : lot_request_id_(lot_request_id) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        int lot_request_id_;
    };

    struct BalanceResponse : public IResponse {
    public:
        BalanceResponse(int rub_balance, int usd_count) : rub_balance_(rub_balance), usd_count_(usd_count) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        int rub_balance_;
        int usd_count_;
    };

    struct AccessAuthResponse : public IResponse {
    public:
        explicit AccessAuthResponse(std::string user_id) : user_id_(std::move(user_id)) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
       std::string user_id_;
    };

    struct UpdateResponse : public IResponse {
    public:
        struct RequestInfo{
            int id;
            int user_id;
            std::string type_of_operation;
            int usd_count;
            int rub_price;
            bool active;
        };

        explicit UpdateResponse(std::vector<std::pair<int, int>> v, std::vector<RequestInfo> reqs) : usd_by_rub_(std::move(v)), requests_(std::move(reqs)) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        std::vector<std::pair<int, int>> usd_by_rub_;
        std::vector<RequestInfo> requests_;
    };

    struct BadResponse : public IResponse {
    public:
        explicit BadResponse(std::string message_text) : message_(std::move(message_text)) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        std::string message_;
    };

    struct Report : public IResponse {
    public:
        explicit Report(int lot_request_id,
                        int usd_count,
                        int rub_price,
                        std::string seller_id,
                        std::string byuer_id) : lot_request_id_(lot_request_id),
                                        usd_count_(usd_count),
                                        rub_price_(rub_price),
                                        seller_name_(std::move(seller_id)),
                                        buyer_name_(std::move(byuer_id)) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        int lot_request_id_;
        int usd_count_;
        int rub_price_;
        std::string seller_name_;
        std::string buyer_name_;
    };
}

#endif //BOURSE_RESPONSE_H
