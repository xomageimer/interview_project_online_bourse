#ifndef BOURSE_CLIENT_H
#define BOURSE_CLIENT_H

#include <QObject>

#include <string>
#include <optional>

#include "gui/mainwindow.h"
#include "bourse/request.h"

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <deque>

struct MainWindow;

namespace network {

    using ssl_socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
    using request_queue_ = std::deque<core::RequestType>;

    enum {
        max_length = 8'192
    };

    struct Client : public QObject{
        Q_OBJECT
    public:
        Client(boost::asio::io_context &io_context,
               boost::asio::ssl::context &context,
               boost::asio::ip::tcp::resolver::results_type endpoints);

        bool verifyCertificate(bool preverified,
                               boost::asio::ssl::verify_context &ctx);

        void write(const core::RequestType &request);

        void close();

        void setUserId(std::string const &user_id);

        void setMainWindow(MainWindow* window);

        [[nodiscard]] std::string const &getUserId() const { return user_id_; }

//        void execResponse(const nlohmann::json &json, MainWindow * mainWindow);
    signals:
        void response(const nlohmann::json & json);

    private:
        void doHandshake(const boost::system::error_code &error);

        void doWrite();

        void doRead();

        ssl_socket socket_;
        std::string user_id_{"John Doe"};
        char data_[max_length];

        request_queue_ pending_requests_;
        char msg_[max_length];

        MainWindow * main_window_;
    };
}

#endif //BOURSE_CLIENT_H
