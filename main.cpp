#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("shoot_commands");
    a.setOrganizationName("shoot_commands");    
    MainWindow w;
    w.show();
    return a.exec();
}
