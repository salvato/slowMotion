#pragma once

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "cameracontrol.h"
#include "commonsettings.h"
#include "preview.h"

#include <stdio.h>


// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT   1
#define MMAL_CAMERA_CAPTURE_PORT 2



class PiCamera
{
public:
    PiCamera();
    ~PiCamera();
    MMAL_STATUS_T createComponent(int cameraNum, int sensorMode);
    MMAL_STATUS_T setCallback();
    MMAL_STATUS_T setConfig(MMAL_PARAMETER_CAMERA_CONFIG_T* pCam_config);
    MMAL_STATUS_T setPortFormats(const RASPICAM_CAMERA_PARAMETERS *camera_parameters,
                                 bool fullResPreview,
                                 MMAL_FOURCC_T encoding,
                                 int width,
                                 int height);
    MMAL_STATUS_T enableCamera();
    void createBufferPool();
    int setAllParameters();
    void destroyComponent();
    MMAL_STATUS_T start(Preview* pPreview);
    void stop();
    void capture();

public:
    MMAL_COMPONENT_T *cameraComponent;// The Camera Component
    CameraControl cameraControl;
    MMAL_POOL_T *pool;

protected:
    MMAL_STATUS_T connectPorts(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection);
    void handleError(MMAL_STATUS_T status, Preview *pPreview);
    void checkDisablePort(MMAL_PORT_T *port);

private:
    MMAL_CONNECTION_T *previewConnection;
};
