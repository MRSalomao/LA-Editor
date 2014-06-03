#include "mainwindow.h"
#include <QDesktopWidget>
#include <QApplication>
#include <QSettings>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QApplication::setStyle(QStyleFactory::create("fusion"));

    QCoreApplication::setOrganizationName("LiberaAkademio");
    QCoreApplication::setOrganizationDomain("LiberaAkademio.com");
    QCoreApplication::setApplicationName("LiberaAkademioEditor");

    MainWindow w;
    w.move(QApplication::desktop()->screen()->rect().center() - w.rect().center());
    w.show();

    return a.exec();
}
