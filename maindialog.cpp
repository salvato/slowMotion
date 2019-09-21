#include <QDebug>
#include "maindialog.h"
#include "ui_maindialog.h"
//#include "linux/gpio.h"


MainDialog::MainDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainDialog)
{
    MMAL_STATUS_T status;
    // Register our application with the logging system
    vcos_log_register("slowMotion", VCOS_LOG_CATEGORY);

    ui->setupUi(this);

    getSensorDefaults(commonSettings.cameraNum,
                      commonSettings.camera_name,
                      &commonSettings.width,
                      &commonSettings.height);
    qDebug() << "Common Parameters Dump:";
    commonSettings.dump_parameters();

    pCamera        = new PiCamera();
    pPreview       = new Preview();// Setup preview window defaults
    pCameraControl = new CameraControl();// Set up the camera_parameters to default

    pCamera->createComponent(commonSettings.cameraNum,
                             commonSettings.sensor_mode);
    pPreview->createComponent();

    // set up the camera configuration
    MMAL_PARAMETER_CAMERA_CONFIG_T camConfig;
    camConfig.hdr = { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(camConfig) };
    camConfig.max_stills_w = uint32_t(commonSettings.width);
    camConfig.max_stills_h = uint32_t(commonSettings.height);
    camConfig.stills_yuv422 = 0;
    camConfig.one_shot_stills = 1;
    camConfig.max_preview_video_w = uint32_t(pPreview->previewWindow.width);
    camConfig.max_preview_video_h = uint32_t(pPreview->previewWindow.height);
    camConfig.num_preview_video_frames = 3;
    camConfig.stills_capture_circular_buffer_height = 0;
    camConfig.fast_preview_resume = 0;
    camConfig.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;
    if(state.fullResPreview) {
        camConfig.max_preview_video_w = uint32_t(commonSettings.width);
        camConfig.max_preview_video_h = uint32_t(commonSettings.height);
    }
    status = pCamera->setConfig(&camConfig);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Could not set sensor configuration: error") << status;
        exit(EXIT_FAILURE);
    }
    // set up Camera Parameters
    int iResult = pCamera->setAllParameters();
    if(iResult != 0) {
        qDebug() << "Unable to set Camera Parameters. error:" << iResult;
        exit(EXIT_FAILURE);
    }
    status = pCamera->setPortFormats(&pCameraControl->cameraParameters,
                                     state.fullResPreview,
                                     state.encoding,
                                     commonSettings.width,
                                     commonSettings.height);
    if(status != MMAL_SUCCESS) {
        qDebug() << "Unable to set Port Formats. error:" << status;
        exit(EXIT_FAILURE);
    }
    status = pCamera->enableCamera();
    if(status != MMAL_SUCCESS) {
        qDebug() << "Unable to Enable Camera. error:" << status;
        exit(EXIT_FAILURE);
    }
    pCamera->createBufferPool();
}


MainDialog::~MainDialog() {
    delete ui;
}



void
MainDialog::getSensorDefaults(int camera_num, char *camera_name, int *width, int *height) {
   MMAL_COMPONENT_T *camera_info;
   MMAL_STATUS_T status;
   // Default to the OV5647 setup
   strncpy(camera_name, "OV5647", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
   // Try to get the camera name and maximum supported resolution
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
   if(status == MMAL_SUCCESS) {
      MMAL_PARAMETER_CAMERA_INFO_T param;
      param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
      param.hdr.size = sizeof(param)-4;  // Deliberately undersize to check firmware version
      status = mmal_port_parameter_get(camera_info->control, &param.hdr);

      if(status != MMAL_SUCCESS) {
         // Running on newer firmware
         param.hdr.size = sizeof(param);
         status = mmal_port_parameter_get(camera_info->control, &param.hdr);
         if(status == MMAL_SUCCESS && param.num_cameras > uint32_t(camera_num)) {
            // Take the parameters from the first camera listed.
            if(*width == 0)
               *width = int32_t(param.cameras[camera_num].max_width);
            if(*height == 0)
               *height = int32_t(param.cameras[camera_num].max_height);
            strncpy(camera_name, param.cameras[camera_num].camera_name, MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
            camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN-1] = 0;
         }
         else
            vcos_log_error("Cannot read camera info, keeping the defaults for OV5647");
      }
      else {
         // Older firmware
         // Nothing to do here, keep the defaults for OV5647
      }
      mmal_component_destroy(camera_info);
   }
   else {
      vcos_log_error("Failed to create camera_info component");
   }
   // default to OV5647 if nothing detected..
   if (*width == 0)
      *width = 2592;
   if (*height == 0)
      *height = 1944;
}



void
MainDialog::on_startButton_clicked() {
    pCamera->start(pPreview);
}


void
MainDialog::on_stopButton_clicked() {
    pCamera->stop();
}
