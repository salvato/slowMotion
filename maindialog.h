#pragma once

#include <QDialog>

#include "picamera.h"
#include "preview.h"
#include "cameracontrol.h"
#include "commonsettings.h"


// Structure containing all state information for the current run
typedef struct {
    int timeout;                        /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
    int timelapse;                      /// Delay between each picture in timelapse mode. If 0, disable timelapse
    int fullResPreview;                 /// If set, the camera preview port runs at capture resolution. Reduces fps.
    int frameNextMethod;                /// Which method to use to advance to next frame
    int burstCaptureMode;               /// Enable burst mode
    int onlyLuma;                       /// Only output the luma / Y plane of the YUV data
    MMAL_FOURCC_T encoding;             /// Use a MMAL encoding other than YUV
    char *linkname;                     /// filename of output file

    MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
    MMAL_COMPONENT_T *null_sink_component; /// Pointer to the camera component
    MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview
    MMAL_POOL_T *camera_pool;              /// Pointer to the pool of buffers used by camera stills port
} APP_STATE;


namespace Ui {
class MainDialog;
}

class MainDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MainDialog(QWidget *parent = nullptr);
    ~MainDialog();

protected:
    void getSensorDefaults(int camera_num, char *camera_name, int *width, int *height);

private slots:
    void on_startButton_clicked();
    void on_stopButton_clicked();

private:
    Ui::MainDialog *ui;
    PiCamera* pCamera;
    Preview* pPreview;
    CameraControl* pCameraControl;
    CommonSettings commonSettings;
    APP_STATE state;
};
