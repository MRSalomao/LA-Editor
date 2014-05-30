#include "mainwindow.h"
#include <QDesktopWidget>
#include <QApplication>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("LiberaAkademio");
    QCoreApplication::setOrganizationDomain("LiberaAkademio.com");
    QCoreApplication::setApplicationName("LiberaAkademioEditor");

    MainWindow w;
    w.adjustSize();
    w.move(QApplication::desktop()->screen()->rect().center() - w.rect().center());
    w.show();

    return a.exec();
}
