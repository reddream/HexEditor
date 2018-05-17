#include "mainwindow.h"
#include <QApplication>

static void setStyle(const QString &style) {
       QFile qss(style);
       qss.open(QFile::ReadOnly);
       qApp->setStyleSheet(qss.readAll());
       qss.close();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("HexEditor");//
    a.setOrganizationName("Organization");//
    setStyle(":/css.qss");
    MainWindow w;
    w.show();

    return a.exec();
}
