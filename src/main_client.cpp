#include <iostream>

#include <QApplication>
#include "gui/mainwindow.h"

int main(int argc, char *argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: bourse_client <host> <port>\n";
            return 1;
        }

        QApplication a(argc, argv);

        MainWindow window{std::string(argv[1]), std::string(argv[2])};
        window.setWindowTitle("Bourse Client");
        window.resize(1000, 900);

        return a.exec();

    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}