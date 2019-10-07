#pragma once

#include <QDialog>
#include <QTimer>
#include <sys/types.h>

#include "picamera.h"
#include "preview.h"
#include "jpegencoder.h"


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
    bool panTiltInit();
    bool setPan(double cameraPanValue);
    bool setTilt(double cameraTiltValue);
    void getSensorDefaults(int camera_num, char *camera_name, int *width, int *height);
    MMAL_STATUS_T setupCameraConfiguration();
    void initDefaults();
    int setDefaultParameters();

private slots:
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_intervalEdit_textEdited(const QString &arg1);
    void on_intervalEdit_editingFinished();
    void on_tTimeEdit_textEdited(const QString &arg1);
    void on_tTimeEdit_editingFinished();
    void onTimeToGetNewImage();
    void on_pathEdit_textChanged(const QString &arg1);
    void on_pathEdit_editingFinished();
    void on_nameEdit_textChanged(const QString &arg1);
    void on_aGainSlider_sliderMoved(int position);
    void on_dGainSlider_sliderMoved(int position);
    void on_aGainSlider_valueChanged(int value);
    void on_dGainSlider_valueChanged(int value);
    void on_dialPan_valueChanged(int value);
    void on_dialTilt_valueChanged(int value);

private:
    Ui::MainDialog* pUi;
    PiCamera*       pCamera;
    Preview*        pPreview;
    JpegEncoder*    pJpegEncoder;

    uint   gpioLEDpin;
    uint   panPin;
    uint   tiltPin;
    double cameraPanValue;
    double cameraTiltValue;
    uint   PWMfrequency;     // in Hz
    int    pulseWidthAt_90;  // in us
    int    pulseWidthAt90;   // in us
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

    int sharpness;             /// -100 to 100
    int contrast;              /// -100 to 100
    int brightness;            ///    0 to 100
    int saturation;            /// -100 to 100
    int ISO;                   ///  TODO : what range?
    int videoStabilisation;    /// 0 or 1 (false or true)
    int exposureCompensation;  ///  -10 to +10 ?
    MMAL_PARAM_EXPOSUREMODE_T exposureMode;
    MMAL_PARAM_EXPOSUREMETERINGMODE_T exposureMeterMode;
    MMAL_PARAM_AWBMODE_T awbMode;
    MMAL_PARAM_IMAGEFX_T imageEffect;
    MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imageEffectsParameters;
    MMAL_PARAM_COLOURFX_T colourEffects;
    MMAL_PARAM_FLICKERAVOID_T flickerAvoidMode;
    int rotation;              /// 0-359
    int hflip;                 /// 0 or 1
    int vflip;                 /// 0 or 1
    PARAM_FLOAT_RECT_T  roi;   /// region of interest to use on the sensor. Normalised [0,1] values in the rect
    int shutter_speed;         /// 0 = auto, otherwise the shutter speed in ms
    float awb_gains_r;         /// AWB red gain
    float awb_gains_b;         /// AWB blue gain
    MMAL_PARAMETER_DRC_STRENGTH_T drc_level;  /// Strength of Dynamic Range compression to apply
    MMAL_BOOL_T stats_pass;    /// Stills capture statistics pass on/off
    int enable_annotate;       /// Flag to enable the annotate, 0 = disabled, otherwise a bitmask of what needs to be displayed
    char annotate_string[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2]; /// String to use for annotate - overrides certain bitmask settings
    int annotate_text_size;    /// Text size for annotation
    int annotate_text_colour;  /// Text colour for annotation
    int annotate_bg_colour;    /// Background colour for annotation
    unsigned int annotate_justify;
    unsigned int annotate_x;
    unsigned int annotate_y;
    MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo_mode;
    float analog_gain;         /// Analog gain
    float digital_gain;        /// Digital gain
    int settings;
    int onlyLuma;              /// Only output the luma / Y plane of the YUV data

    int fullResPreview;        /// If set, the camera preview port runs at capture resolution. Reduces fps.
    MMAL_FOURCC_T encoding;    /// Use a MMAL encoding other than YUV
};
