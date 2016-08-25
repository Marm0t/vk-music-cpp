#include "mainwindow.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    //qputenv("QT_LOGGING_RULES", "qt.network.ssl.warning=false");
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
