#pragma once


#include "interface/mmal/mmal_types.h"
#include "interface/mmal/mmal_parameters_camera.h"



class CommonSettings
{
public:
    CommonSettings();

public:
    void set_defaults();
    void dump_parameters();

public:
    char camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN]; /// Name of the camera sensor
    int width;       /// Requested width of image
    int height;      /// requested height of image
    char *filename;  /// filename of output file
    int cameraNum;   /// Camera number
    int sensor_mode; /// Sensor mode. 0=auto. Check docs/forum for modes selected by other values.
    int verbose;     /// !0 if want detailed run information
    int gps;         /// Add real-time gpsd output to output
};

