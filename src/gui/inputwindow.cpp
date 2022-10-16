#include "inputwindow.h"
#include "ui_inputwindow.h"

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

#include "mainwindow.h"

InputWindow::InputWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::InputWindow)
{
    ui->setupUi(this);
}

void InputWindow::on_loginButton_clicked()
{
    usd_count = ui->loginInput->text().toStdString();
    rub_price = ui->passwordInput->text().toStdString();
    emit set_price(std::stoi(usd_count), std::stoi(rub_price));
    this->hide();
}

InputWindow::~InputWindow()
{
    delete ui;
}
