#include "mainwindow.h"
#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFont font("Segoe UI", 10);
    a.setFont(font);

    a.setApplicationName("SensorChain");
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName("IoT Blockchain");

    MainWindow w;
    w.show();

    return a.exec();
}
