#include "authwindow.h"
#include "ui_authwindow.h"

#include <QtWidgets/QMainWindow>
#include <QtCharts/QChartView>

#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QLegend>

#include <QtCharts/QBarCategoryAxis>

#include <QtCharts/QHorizontalStackedBarSeries>

#include <QtCharts/QLineSeries>

#include <QtCharts/QCategoryAxis>

#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>

QT_CHARTS_USE_NAMESPACE

#include <QDebug>

#include "network/client.h"

#include "mainwindow.h"

AuthWindow::AuthWindow(MainWindow & mainwindow, QWidget *parent) :
    main_window(mainwindow),
    QMainWindow(parent),
    ui(new Ui::AuthWindow)
{
    ui->setupUi(this);
}

AuthWindow::~AuthWindow()
{
    delete ui;
}

void AuthWindow::on_loginButton_clicked()
{
    auto req = std::make_shared<core::AuthorizationRequest>(ui->loginInput->text().toStdString(),
                                                            ui->passwordInput->text().toStdString());
    main_window.getClient()->write(std::move(req));
}

void AuthWindow::on_registerButton_clicked()
{
    auto req = std::make_shared<core::RegistrationRequest>(ui->loginInput->text().toStdString(),
        ui->passwordInput->text().toStdString());
    main_window.getClient()->write(std::move(req));
}
