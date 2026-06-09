#include "main_window.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("EGG-Delivery"));
    QApplication::setOrganizationName(QStringLiteral("EGG-Delivery"));

    MainWindow window;
    window.show();

    return app.exec();
}
