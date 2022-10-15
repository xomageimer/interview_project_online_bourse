#ifndef BOURSE_REQUEST_H
#define BOURSE_REQUEST_H

#include "database_manager.h"

#include "nlohmann/json.hpp"

#include <deque>
#include <memory>
#include <utility>

namespace core {
    using RequestType = std::shared_ptr<struct IRequest>;

    enum RequestAction {
        SELL_USD_REQUEST = 0,
        BUY_USD_REQUEST,
        GET_BALANCE_REQUEST,
        CANCEL_REQUEST,
        AUTHORIZATION_REQUEST,
        REGISTER_REQUEST,
        UPDATE_REQUEST,
    };

    struct IRequest {
    public:
        explicit IRequest(std::string  user_id) : user_id_(std::move(user_id)) {}

        [[nodiscard]] virtual nlohmann::json getJson() const = 0;

        virtual ~IRequest() = default;

    protected:
       std::string user_id_;
    };

    struct CreateLotRequest : public IRequest {
    public:
        enum class OperationType {
            BUY_USD,
            SELL_USD
        };

        CreateLotRequest(const std::string& user_id, int usd_count, int rub_price, OperationType op) : IRequest(user_id), usd_count_(usd_count), rub_price_(rub_price),
                                                                           operation_(op) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        int usd_count_;
        int rub_price_;
        OperationType operation_;
    };

    struct BalanceRequest : public IRequest {
    public:
        explicit BalanceRequest(std::string const & user_id) : IRequest(user_id) {}

        [[nodiscard]] nlohmann::json getJson() const override;
    };

    struct CancelRequest : public IRequest {
    public:
        explicit CancelRequest(std::string const & user_id, int request_id) : IRequest(user_id), request_id_(request_id) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        int request_id_;
    };

    struct AuthorizationRequest : public IRequest {
    public:
        AuthorizationRequest(std::string name, std::string password) : IRequest("John Doe"), name_(std::move(name)),
                                                                       password_(std::move(password)) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        std::string name_;
        std::string password_;
    };

    struct RegistrationRequest : public IRequest {
    public:
        RegistrationRequest(std::string name, std::string password) : IRequest("John Doe"),  name_(std::move(name)),
                                                                      password_(std::move(password)) {}

        [[nodiscard]] nlohmann::json getJson() const override;

    private:
        std::string name_;
        std::string password_;
    };

    struct UpdateRequest : public IRequest {
    public:
        using IRequest::IRequest;

        [[nodiscard]] nlohmann::json getJson() const override;
    };

    std::shared_ptr<IRequest> makeRequest(std::string const & msg);
}

#endif //BOURSE_REQUEST_H
