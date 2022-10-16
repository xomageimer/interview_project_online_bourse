#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <string>
#include <thread>

#include <QMainWindow>
#include <QWidget>

#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QStringListModel>
#include <QPushButton>
#include <QMessageBox>

#include <QtWidgets/QMainWindow>
#include <QtCharts/QChartView>

#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QLegend>

#include <QtCharts/QBarCategoryAxis>

#include <QtCharts/QHorizontalStackedBarSeries>

#include <QtCharts/QLineSeries>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "nlohmann/json.hpp"

QT_CHARTS_USE_NAMESPACE

namespace Ui {
class MainWindow;
}
class AuthWindow;
namespace network{
    struct Client;
}
namespace core{
    struct CreateLotRequest;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const std::string & host, const std::string & port, QWidget *parent = 0);
    ~MainWindow();

    std::shared_ptr<network::Client> getClient();

    void showBadMessage(std::string const & msg);

    AuthWindow * getAuthWindow();

    void updateCharts(const std::vector<std::pair<int, int>> & v);

public slots:
    void execResponse(const nlohmann::json &json);

private slots:
    void on_updateButton_clicked();

private:
    Ui::MainWindow *ui;
    QStringListModel *Model;
    QListView *ListView;
    QPushButton *SellButton, *DeleteButton, *BuyButton;
    QStringList List;

    AuthWindow * auth_window;

    boost::asio::io_context io_context;
    std::shared_ptr<network::Client> client;
    std::thread context_worker;

    QChart *chart;
    QChartView *chartView;

    QListView *ReportView;
    QStringListModel *ReportModel;

    std::map<int, std::shared_ptr<core::CreateLotRequest>> to_add_requests;
public slots:
    void sell_clicked();
    void delete_clicked();
    void buy_clicked();
    void showSelectedItem(QModelIndex mIndex);
};

#endif // MAINWINDOW_H
