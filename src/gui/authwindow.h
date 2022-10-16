#ifndef AUTHWINDOW_H
#define AUTHWINDOW_H

#include <QMainWindow>
#include <QWidget>

#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QStringListModel>
#include <QPushButton>
#include <QMessageBox>

namespace Ui {
class AuthWindow;
}
class MainWindow;

class AuthWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AuthWindow(MainWindow & mainwindow, QWidget *parent = 0);
    ~AuthWindow();

private slots:
    void on_loginButton_clicked();

    void on_registerButton_clicked();

private:
    Ui::AuthWindow *ui;
    MainWindow & main_window;

};

#endif // AUTHWINDOW_H
