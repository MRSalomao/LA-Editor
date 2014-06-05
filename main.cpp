#include "mainwindow.h"
#include <QDesktopWidget>
#include <QApplication>
#include <QSettings>
#include <QStyleFactory>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setStyle(QStyleFactory::create("fusion"));

    QCoreApplication::setOrganizationName("LiberaAkademio");
    QCoreApplication::setOrganizationDomain("LiberaAkademio.com");
    QCoreApplication::setApplicationName("LiberaAkademioEditor");

    MainWindow window;
    window.move(QApplication::desktop()->screen()->rect().center() - window.rect().center());
    window.show();

    return app.exec();
}
