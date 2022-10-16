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

    [[nodiscard]] std::pair<std::string, std::string> getData() const;

private slots:
    void on_loginButton_clicked();

private:
    Ui::InputWindow *ui;

    std::string usd_count, rub_price;
};

#endif // INPUTWINDOW_H
