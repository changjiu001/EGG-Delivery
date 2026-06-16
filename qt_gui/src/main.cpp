#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app_icon.xpm")));
    MainWindow window;
    window.show();
    return app.exec();
}
