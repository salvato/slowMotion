#include "maindialog.h"
#include "utility.h"
#include "bcm_host.h"
#include <QApplication>
#include "utility.h"


int
main(int argc, char *argv[]) {
    bcm_host_init();
    if(verbose)
        checkConfiguration(128);
    QApplication a(argc, argv);
    MainDialog w;
    w.show();
    return a.exec();
}
