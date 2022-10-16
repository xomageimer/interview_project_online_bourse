#ifndef INPUTWINDOW_H
#define INPUTWINDOW_H

#include <QMainWindow>
#include <QWidget>

#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QStringListModel>
#include <QPushButton>
#include <QMessageBox>

namespace Ui {
class InputWindow;
}
class MainWindow;

class InputWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit InputWindow(QWidget *parent = 0);
    ~InputWindow();

signals:
    void set_price(int usd, int price);

private slots:
    void on_loginButton_clicked();

private:
    Ui::InputWindow *ui;

    std::string usd_count {"1"}, rub_price{"65"};
};

#endif // INPUTWINDOW_H
