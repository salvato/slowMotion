#include "commonsettings.h"
#include <memory.h>
#include <QDebug>


CommonSettings::CommonSettings() {
    strncpy(camera_name, "(Unknown)", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
    // We dont set width and height since these will be specific to the app being built.
    width       = 0;
    height      = 0;
    filename    = nullptr;
    verbose     = 0;
    cameraNum   = 0;
    sensor_mode = 3;
    gps         = 0;
}


void
CommonSettings::dump_parameters() {
   qDebug() << QString("Camera Name %1")
               .arg(camera_name);
   qDebug() << QString("Width %1, Height %2, filename %3")
               .arg(width)
               .arg(height)
               .arg(filename);
   qDebug() << QString("Using camera %1, sensor mode %2")
               .arg(cameraNum)
               .arg(sensor_mode);
   qDebug() << QString("GPS output %1")
               .arg(gps ? "Enabled" : "Disabled");
}
