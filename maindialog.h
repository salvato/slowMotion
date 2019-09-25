#pragma once

#include <QDialog>
#include <QTimer>
#include <sys/types.h>

#include "picamera.h"
#include "preview.h"
#include "cameracontrol.h"
#include "setupdialog.h"


namespace Ui {
class MainDialog;
}

class MainDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MainDialog(QWidget *parent = nullptr);
    ~MainDialog() Q_DECL_OVERRIDE;

protected:
    void setupStyles();
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    void moveEvent(QMoveEvent *event) Q_DECL_OVERRIDE;
    void restoreSettings();
    void dumpParameters();
    void switchLampOn();
    void switchLampOff();
    bool checkValues();
    bool gpioInit();
    void getSensorDefaults(int camera_num, char *camera_name, int *width, int *height);
    MMAL_STATUS_T setupCameraConfiguration();

private slots:
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_setupButton_clicked();
    void on_intervalEdit_textEdited(const QString &arg1);
    void on_intervalEdit_editingFinished();
    void on_tTimeEdit_textEdited(const QString &arg1);
    void on_tTimeEdit_editingFinished();
    void onTimeToGetNewImage();
    void on_pathEdit_textChanged(const QString &arg1);
    void on_pathEdit_editingFinished();
    void on_nameEdit_textChanged(const QString &arg1);

private:
    Ui::MainDialog* pUi;
    PiCamera* pCamera;
    Preview* pPreview;
    CameraControl* pCameraControl;
    setupDialog*    pSetupDlg;

    uint   gpioLEDpin;
    uint   panPin;
    uint   tiltPin;
    double cameraPanAngle;
    double cameraTiltAngle;
    uint   PWMfrequency;     // in Hz
    double pulseWidthAt_90;  // in us
    double pulseWidthAt90;   // in us
    int    gpioHostHandle;

    int    msecInterval;
    int    secTotTime;
    int    imageNum;

    QString sNormalStyle;
    QString sErrorStyle;
    QString sDarkStyle;
    QString sPhotoStyle;
    QString sBlackStyle;

    QString sBaseDir;
    QString sOutFileName;

    QTimer intervalTimer;

    QPoint dialogPos;
    QPoint videoPos;
    QSize videoSize;

    char cameraName[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN]; /// Name of the camera sensor
    int width;       /// Requested width of image
    int height;      /// requested height of image
    char *filename;  /// filename of output file
    int cameraNum;   /// Camera number
    int sensorMode; /// Sensor mode. 0=auto. Check docs/forum for modes selected by other values.
    int gps;         /// Add real-time gpsd output to output

    int fullResPreview;                 /// If set, the camera preview port runs at capture resolution. Reduces fps.
    MMAL_FOURCC_T encoding;             /// Use a MMAL encoding other than YUV
};
