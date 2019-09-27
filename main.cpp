#include "maindialog.h"
#include "utility.h"
#include "bcm_host.h"
#include <QApplication>


int
main(int argc, char *argv[]) {
    bcm_host_init();
    checkConfiguration(128);
    QApplication a(argc, argv);
    MainDialog w;
    w.show();
    return a.exec();
}
