#pragma once

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "cameracontrol.h"
#include "preview.h"
#include "jpegencoder.h"

#include <stdio.h>
#include <QString>


// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT   1
#define MMAL_CAMERA_CAPTURE_PORT 2



//struct {
//   const char *format;
//   MMAL_FOURCC_T encoding;
//} encoding_xref[] = {
//   {"jpg", MMAL_ENCODING_JPEG},
//   {"bmp", MMAL_ENCODING_BMP},
//   {"gif", MMAL_ENCODING_GIF},
//   {"png", MMAL_ENCODING_PNG},
//   {"ppm", MMAL_ENCODING_PPM},
//   {"tga", MMAL_ENCODING_TGA}
//};


class PiCamera
{
public:
    PiCamera(int cameraNum, int sensorMode);
    ~PiCamera();
    MMAL_STATUS_T setConfig(MMAL_PARAMETER_CAMERA_CONFIG_T* pCam_config);
    MMAL_STATUS_T setPortFormats(bool fullResPreview,
                                 MMAL_FOURCC_T encoding,
                                 int width,
                                 int height);
    MMAL_STATUS_T enableCamera();
    void createBufferPool();
    void destroyComponent();
    MMAL_STATUS_T startPreview(Preview *pPreview);
    MMAL_STATUS_T start(JpegEncoder* pEncoder);
    void stop(JpegEncoder *pEncoder);
    void capture(QString sPathName);

public:
    MMAL_COMPONENT_T *component;// The Camera Component
    CameraControl *pControl;
    MMAL_POOL_T *pool;

protected:
    MMAL_STATUS_T createComponent(int cameraNum, int sensorMode);
    MMAL_STATUS_T connectPorts(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection);
    void handleError(MMAL_STATUS_T status, Preview *pPreview);
    void checkDisablePort(MMAL_PORT_T *port);
    void set_defaults();

private:
    MMAL_CONNECTION_T *previewConnection;
    MMAL_CONNECTION_T *encoderConnection;
};
