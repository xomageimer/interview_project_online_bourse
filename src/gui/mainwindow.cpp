#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>

#include "inputwindow.h"
#include "authwindow.h"

#include <iostream>

#include "network/client.h"
#include "bourse/response.h"
#include "utils/auxiliary.h"

MainWindow::MainWindow(const std::string &host, const std::string &port, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), auth_window(new AuthWindow(*this)), input_window(new InputWindow()) {
    boost::asio::ip::tcp::resolver resolver(io_context);
    boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, port);

    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.load_verify_file("server_certificate.crt");

    client = std::make_shared<network::Client>(io_context, ctx, endpoints);
    client->setMainWindow(this);

    qRegisterMetaType<nlohmann::json>("nlohmann::json");

    connect(client.get(), &network::Client::response, this, &MainWindow::execResponse);

    context_worker = std::thread([this]() { io_context.run(); });

    ui->setupUi(this);

    ui->label->setText("");

    ListView = new QListView;
    Model = new QStringListModel(this);
    Model->setStringList(List);
    ListView->setModel(Model);
    ListView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    SellButton = new QPushButton("Купить");
    DeleteButton = new QPushButton("Отменить");
    BuyButton = new QPushButton("Продать");

    ui->grid_Layout->addWidget(ListView, 2, 0, 2, 3);
    ui->grid_Layout->addWidget(SellButton, 4, 0);
    ui->grid_Layout->addWidget(DeleteButton, 4, 1);
    ui->grid_Layout->addWidget(BuyButton, 4, 2);

    connect(SellButton, SIGNAL(clicked()), this, SLOT(sell_clicked()));
    connect(DeleteButton, SIGNAL(clicked()), this, SLOT(delete_clicked()));
    connect(BuyButton, SIGNAL(clicked()), this, SLOT(buy_clicked()));

    chart = new QChart();
    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    ui->horizontalLayout->addWidget(chartView);
    chartView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    chartView->show();

    ReportView = new QListView;
    ReportModel = new QStringListModel(this);
    ReportModel->setStringList(List);
    ReportView->setModel(ReportModel);
    ReportView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->horizontalLayout->addWidget(ReportView);
    ReportView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    this->hide();
    auth_window->show();
}

MainWindow::~MainWindow() {
    client->close();
    context_worker.join();
    delete ui;
    delete auth_window;
    delete input_window;
}

void MainWindow::sell_clicked() {
    input_window->show();
    auto [usd, price] = input_window->getData();
    auto req = std::make_shared<core::CreateLotRequest>(
        getClient()->getUserId(), std::stoi(usd), std::stoi(price), core::CreateLotRequest::OperationType::SELL_USD);
    to_add_requests.emplace(Model->rowCount(), req);
    getClient()->write(std::move(req));
}

void MainWindow::delete_clicked() {
    auto req = std::make_shared<core::CancelRequest>(getClient()->getUserId(), idx_by_lot.at(ListView->currentIndex().row()));
    getClient()->write(std::move(req));
}

void MainWindow::buy_clicked() {
    input_window->show();
    auto [usd, price] = input_window->getData();
    auto req = std::make_shared<core::CreateLotRequest>(
        getClient()->getUserId(), std::stoi(usd), std::stoi(price), core::CreateLotRequest::OperationType::BUY_USD);
    to_add_requests.emplace(Model->rowCount(), req);
    getClient()->write(std::move(req));
}

void MainWindow::on_updateButton_clicked() {
    chart->removeAllSeries();
    auto req = std::make_shared<core::UpdateRequest>(getClient()->getUserId());
    getClient()->write(std::move(req));
}

std::shared_ptr<network::Client> MainWindow::getClient() {
    return client;
}

AuthWindow *MainWindow::getAuthWindow() {
    return auth_window;
}

void MainWindow::showBadMessage(const std::string &msg) {
    ui->label->setText(QString::fromStdString("last request return: " + msg));
}

void MainWindow::updateCharts(const std::vector<std::pair<int, int>> &v) {
    auto *series = new QLineSeries();

    for (auto [usd, rub] : v) {
        series->append(usd, rub);
    }

    chart->legend()->hide();
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->setTitle("Timestamp / RUB price");

    chartView->show();
}

void MainWindow::execResponse(const nlohmann::json &json) {
    switch (json["response_type"].get<int>()) {
        case core::ResponseAction::SUCCESS_SET_RESPONSE: {
            auto idx = json["lot_id"].get<int>();
            if (json["is_to_cancel"].get<bool>()) {
                Model->removeRows(lot_by_idx.at(idx), 1);
                idx_by_lot.erase(lot_by_idx.at(idx));
                lot_by_idx.erase(idx);
            } else {
                int row = Model->rowCount();
                Model->insertRows(row, 1);
                QModelIndex index = Model->index(row);
                ListView->setCurrentIndex(index);
                auto lot_data_json = to_add_requests.at(idx)->getJson();
                Model->setData(index, QString::fromStdString(std::string("Request to ") +
                                                             std::string((lot_data_json["request_type"].get<int>() ==
                                                                             core::CreateLotRequest::OperationType::SELL_USD)
                                                                             ? "sell "
                                                                             : "buy ") +
                                                             std::to_string(lot_data_json["usd_count"].get<int>()) + "$ for " +
                                                             std::to_string(lot_data_json["rub_price"].get<int>()) + "₽"));
                lot_by_idx.emplace(idx, row);
                idx_by_lot.emplace(row, idx);
            }
            break;
        }
        case core::ResponseAction::BAD_RESPONSE:
            showBadMessage(json["message"].get<std::string>());
            break;
        case core::ResponseAction::GET_BALANCE_RESPONSE:
            ui->label_2->setText(QString::fromStdString("Balance: " + std::to_string(json["usd_count"].get<int>()) + "$\t" +
                                                        std::to_string(json["rub_balance"].get<int>()) + "₽"));
            break;
        case core::ResponseAction::AUTHORIZATION_RESPONSE:
            getClient()->setUserId(unquoted(json["user_id"].get<std::string>()));
            show();
            getAuthWindow()->hide();
            break;
        case core::ResponseAction::UPDATE_RESPONSE: {
            std::vector<std::pair<int, int>> quotation;
            for (auto &a : json["quotation"]) {
                quotation.emplace_back(a[0], a[1]);
            }
            updateCharts(quotation);
            break;
        }
        case core::ResponseAction::REPORT: {
            int row = ReportModel->rowCount();
            ReportModel->insertRows(row, 1);
            QModelIndex index = ReportModel->index(row);
            ReportModel->setData(
                index, QString::fromStdString(std::to_string(json["lot_id"].get<int>()) + " : " + json["seller_name"].get<std::string>() +
                                              " sold " + std::to_string(json["usd_count"].get<int>()) + "$ to " +
                                              json["buyer_name"].get<std::string>() + " at " +
                                              std::to_string(json["rub_price"].get<int>()) + "₽ apiece"));
            break;
        }
        default:
            showBadMessage("wrong response type, mb server was hacking!");
            break;
    }
}
