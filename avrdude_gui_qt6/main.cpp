#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AVRDUDE GUI");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("avrdudes");

    MainWindow w;
    w.show();
    return app.exec();
}
