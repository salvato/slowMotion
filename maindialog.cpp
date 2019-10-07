#include <QDebug>
#include "maindialog.h"
#include "ui_maindialog.h"
#include "pigpiod_if2.h"// The library for using GPIO pins on Raspberry
#include <QThread>
#include <QMoveEvent>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include <QDir>
#include "utility.h"


#define MIN_INTERVAL 1500 // in ms (depends on the image format: jpeg is HW accelerated !)
#define IMAGE_QUALITY 100 // 100 is Best quality


// ================================================
// GPIO Numbers are Broadcom (BCM) numbers
// ================================================
// +5V on pins 2 or 4 in the 40 pin GPIO connector.
// GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
// in the 40 pin GPIO connector.
// ================================================
#define LED_PIN  23 // BCM23 is Pin 16 in the 40 pin GPIO connector.
#define PAN_PIN  14 // BCM14 is Pin  8 in the 40 pin GPIO connector.
#define TILT_PIN 26 // BCM26 IS Pin 37 in the 40 pin GPIO connector.


MainDialog::MainDialog(QWidget *parent)
    : QDialog(parent)
    , pUi(new Ui::MainDialog)
    , gpioLEDpin(LED_PIN)
    , panPin(PAN_PIN)
    , tiltPin(TILT_PIN)
    , gpioHostHandle(-1)
    , width(0)
    , height(0)
    , filename(nullptr)
    , cameraNum(0)
    , sensorMode(3)
    , gps(0)
{
    MMAL_STATUS_T status;
    if(verbose)
        exit(EXIT_FAILURE);

    pUi->setupUi(this);
    setFixedSize(size());
// Determine the Preview Window Position
    dialogPos = pos();
    videoPos  = pUi->labelVideo->pos();
    videoSize = pUi->labelVideo->size();
// Setup the QLineEdit visual styles
    setupStyles();
// Init GPIO handling
    if(!gpioInit())
        exit(EXIT_FAILURE);
    // Values to be checked with the used servos
    PWMfrequency    =   50; // in Hz
    pulseWidthAt_90 =  600; // in us
    pulseWidthAt90  = 2200; // in us
    pUi->dialPan->setRange(pulseWidthAt_90, pulseWidthAt90);
    pUi->dialTilt->setRange(pulseWidthAt_90, pulseWidthAt90);
    cameraPanValue  = (pulseWidthAt90-pulseWidthAt_90)/2+pulseWidthAt_90;
    cameraTiltValue = cameraPanValue;
// Init GPIOs
    if(!panTiltInit())
        exit(EXIT_FAILURE);

    analog_gain = 1.0;
    digital_gain = 1.0;
// Prepare for periodic image acquisition
    switchLampOn();
    intervalTimer.stop();// Probably non needed but...does'nt hurt
    connect(&intervalTimer,
            SIGNAL(timeout()),
            this,
            SLOT(onTimeToGetNewImage()));
// Check for the presence of the Pi Camera
    getSensorDefaults(cameraNum, cameraName, &width, &height);
    if(verbose)
        dumpParameters();
// Create the needed Components
    pCamera        = new PiCamera(cameraNum, sensorMode);
    pPreview       = new Preview(videoSize.width(), videoSize.height());// Setup preview window defaults
    pJpegEncoder   = new JpegEncoder();
// Set up the Camera Configuration
    if(setupCameraConfiguration() != MMAL_SUCCESS)
        exit(EXIT_FAILURE);
// Set up default Camera Parameters
    initDefaults();
    int iResult = setDefaultParameters();
    if(iResult != 0) {
        qDebug() << "Unable to set Camera Parameters. error:" << iResult;
        exit(EXIT_FAILURE);
    }
// Set up the Camera Port formats
    status = pCamera->setPortFormats(fullResPreview,
                                     encoding,
                                     width,
                                     height);
    if(status != MMAL_SUCCESS) {
        qDebug() << "Unable to set Port Formats. error:" << status;
        exit(EXIT_FAILURE);
    }
// Vertical Flip of the image
    pCamera->pControl->set_flips(0, 1);
// Enable the Camera processing
    status = pCamera->enableCamera();
    if(status != MMAL_SUCCESS) {
        qDebug() << "Unable to Enable Camera. error:" << status;
        exit(EXIT_FAILURE);
    }
// Restore Previous Dialog Values
    restoreSettings();
// Init User Interface with restored values
    pUi->pathEdit->setText(sBaseDir);
    pUi->nameEdit->setText(sOutFileName);
    pUi->startButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval));
    pUi->tTimeEdit->setText(QString("%1").arg(secTotTime));
    pUi->labelVideo->setStyleSheet(sBlackStyle);
    pUi->aGainSlider->setValue(int(analog_gain*10.0f));
    pUi->dGainSlider->setValue(int(digital_gain*10.0f));
    pUi->dialPan->setValue(int(cameraPanValue));
    pUi->dialTilt->setValue(int(cameraTiltValue));
// Enable the Camera processing
    status = pCamera->startPreview(pPreview);
    if(status != MMAL_SUCCESS) {
        qDebug() << "Unable to Start Camera Preview. error:" << status;
        exit(EXIT_FAILURE);
    }
    imageNum = 0;
}


void
MainDialog::setupStyles() {
    sNormalStyle = pUi->lampStatus->styleSheet();
    sErrorStyle  = "QLineEdit { \
                        color: rgb(255, 255, 255); \
                        background: rgb(255, 0, 0); \
                        selection-background-color: rgb(128, 128, 255); \
                    }";
    sDarkStyle   = "QLineEdit { \
                        color: rgb(255, 255, 255); \
                        background: rgb(0, 0, 0); \
                        selection-background-color: rgb(128, 128, 255); \
                    }";
    sPhotoStyle  = "QLineEdit { \
                        color: rgb(0, 0, 0); \
                        background: rgb(255, 255, 0); \
                        selection-background-color: rgb(128, 128, 255); \
                    }";

    sBlackStyle   = "QLabel { \
                        color: rgb(255, 255, 255); \
                        background: rgb(0, 0, 0); \
                        selection-background-color: rgb(128, 128, 255); \
                    }";
}


void
MainDialog::closeEvent(QCloseEvent *event) {
    Q_UNUSED(event)
    intervalTimer.stop();
    switchLampOff();
    // Save settings
    QSettings settings;
    settings.setValue("BaseDir", sBaseDir);
    settings.setValue("FileName", sOutFileName);
    settings.setValue("Interval", msecInterval);
    settings.setValue("TotalTime", secTotTime);
    settings.setValue("AnalogGain", analog_gain);
    settings.setValue("DigitalGain", digital_gain);
    settings.setValue("panValue",  cameraPanValue);
    settings.setValue("tiltValue", cameraTiltValue);
    // Free GPIO
    if(gpioHostHandle >= 0)
        pigpio_stop(gpioHostHandle);
}


MainDialog::~MainDialog() {
    delete pUi;
}


void
MainDialog::moveEvent(QMoveEvent *event) {
    dialogPos = event->pos();
    videoPos  = pUi->labelVideo->pos();
    videoSize = pUi->labelVideo->size();
    MMAL_RECT_T previewWindow = {dialogPos.x()+videoPos.x(),
                                 dialogPos.y()+videoPos.y(),
                                 videoSize.width(),
                                 videoSize.height()};
    pPreview->setScreenPos(previewWindow);
}


void
MainDialog::restoreSettings() {
    QSettings settings;
    sBaseDir     = settings.value("BaseDir",
                                  QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    sOutFileName = settings.value("FileName",
                                  QString("test")).toString();
    msecInterval    = settings.value("Interval", 10000).toInt();
    secTotTime      = settings.value("TotalTime", 0).toInt();
    analog_gain     = settings.value("AnalogGain", 1).toFloat();
    digital_gain    = settings.value("DigitalGain", 1).toFloat();
    cameraPanValue  = settings.value("panValue",  cameraPanValue).toDouble();
    cameraTiltValue = settings.value("tiltValue", cameraTiltValue).toDouble();
}


bool
MainDialog::panTiltInit() {
    int iResult;
    // Camera Pan-Tilt Control
    iResult = set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Non riesco a definire la frequenza del PWM per il Pan."));
        return false;
    }
    if(!setPan(cameraPanValue))
        return false;
    if(!setTilt(cameraTiltValue))
        return false;
    return true;
}


bool
MainDialog::setPan(double cameraPanValue) {
    double pulseWidth = cameraPanValue;// In us
    int iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
    if(iResult < 0) {
        QString sError;
        if(iResult == PI_BAD_USER_GPIO)
            sError = QString("Bad User GPIO");
        else if(iResult == PI_BAD_PULSEWIDTH)
            sError = QString("Bad Pulse Width %1").arg(pulseWidth);
        else if(iResult == PI_NOT_PERMITTED)
            sError = QString("Not Permitted");
        else
            sError = QString("Unknown Error");
        QMessageBox::critical(this,
                              sError,
                              QString("Non riesco a far partire il PWM per il Pan."));
        return false;
    }
    set_PWM_frequency(gpioHostHandle, panPin, 0);
    iResult = set_PWM_frequency(gpioHostHandle, tiltPin, 0);
    if(iResult == PI_BAD_USER_GPIO) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Bad User GPIO"));
        return false;
    }
    if(iResult == PI_NOT_PERMITTED) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("GPIO operation not permitted"));
        return false;
    }
    return true;
}


bool
MainDialog::setTilt(double cameraTiltValue) {
    double pulseWidth = cameraTiltValue;// In us
    int iResult = set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Non riesco a definire la frequenza del PWM per il Tilt."));
        return false;
    }
    iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
    if(iResult < 0) {
        QString sError;
        if(iResult == PI_BAD_USER_GPIO)
            sError = QString("Bad User GPIO");
        else if(iResult == PI_BAD_PULSEWIDTH)
            sError = QString("Bad Pulse Width %1").arg(pulseWidth);
        else if(iResult == PI_NOT_PERMITTED)
            sError = QString("Not Permitted");
        else
            sError = QString("Unknown Error");
        QMessageBox::critical(this,
                              sError,
                              QString("Non riesco a far partire il PWM per il Tilt."));
        return false;
    }
    iResult = set_PWM_frequency(gpioHostHandle, tiltPin, 0);
    if(iResult == PI_BAD_USER_GPIO) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Bad User GPIO"));
        return false;
    }
    if(iResult == PI_NOT_PERMITTED) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("GPIO operation not permitted"));
        return false;
    }
    return true;
}


int
MainDialog::setDefaultParameters() {
    int result;
    CameraControl* pCameraControl = pCamera->pControl;
    result  = pCameraControl->set_saturation(saturation);
    result += pCameraControl->set_sharpness(sharpness);
    result += pCameraControl->set_contrast(contrast);
    result += pCameraControl->set_brightness(brightness);
    result += pCameraControl->set_ISO(ISO);
    result += pCameraControl->set_video_stabilisation(videoStabilisation);
    result += pCameraControl->set_exposure_compensation(exposureCompensation);
    result += pCameraControl->set_exposure_mode(exposureMode);
    result += pCameraControl->set_flicker_avoid_mode(flickerAvoidMode);
    result += pCameraControl->set_metering_mode(exposureMeterMode);
    result += pCameraControl->set_awb_mode(awbMode);
    result += pCameraControl->set_awb_gains(awb_gains_r, awb_gains_b);
    result += pCameraControl->set_imageFX(imageEffect);
    result += pCameraControl->set_colourFX(&colourEffects);
    //result += pCameraControl->set_thumbnail_parameters(&thumbnailConfig);  TODO Not working for some reason
    result += pCameraControl->set_rotation(rotation);
    result += pCameraControl->set_flips(hflip, vflip);
    result += pCameraControl->set_ROI(roi);
    result += pCameraControl->set_shutter_speed(shutter_speed);
    result += pCameraControl->set_DRC(drc_level);
    result += pCameraControl->set_stats_pass(stats_pass);
    result += pCameraControl->set_annotate(enable_annotate,
                                           annotate_string,
                                           annotate_text_size,
                                           annotate_text_colour,
                                           annotate_bg_colour,
                                           annotate_justify,
                                           annotate_x,
                                           annotate_y);
    result += pCameraControl->set_gains(analog_gain, digital_gain);
    if(settings) {
        MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request = {
            {MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
            MMAL_PARAMETER_CAMERA_SETTINGS, 1
        };
        MMAL_STATUS_T status = mmal_port_parameter_set(pCamera->component->control, &change_event_request.hdr);
        if(status != MMAL_SUCCESS) {
            qDebug() << QString("No camera settings events");
        }
        result += status;
    }
    return result;
}


//  Give a set of default values
void
MainDialog::initDefaults() {
    sharpness             = 0;// -100 - 100 (image sharpness; 0 default)
    contrast              = 0;// -100 - 100 (image contrast; 0 default)
    brightness            = 50;// 0 - 100 (image brightness; 50 default)
    saturation            = 0;// -100 - 100 (image saturation; 0 default)
    ISO                   = 0;// 100 - 800 (0 = auto)
    videoStabilisation    = 0;// 0=false, not 0=true
    exposureCompensation  = 0;// -10 - 10 (exposure Compensation; 0 default)
    exposureMode          = MMAL_PARAM_EXPOSUREMODE_FIXEDFPS;//MMAL_PARAM_EXPOSUREMODE_AUTO;//
    flickerAvoidMode      = MMAL_PARAM_FLICKERAVOID_OFF;
    exposureMeterMode     = MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE;
    awbMode               = MMAL_PARAM_AWBMODE_AUTO;//MMAL_PARAM_AWBMODE_OFF;//
    imageEffect           = MMAL_PARAM_IMAGEFX_NONE;
    colourEffects.enable  = 0;
    colourEffects.u       = 128;
    colourEffects.v       = 128;
    rotation              = 0;
    hflip                 = 0;
    vflip                 = 0;
    roi.x                 = 0.0;
    roi.y                 = 0.0;
    roi.w                 = 1.0;
    roi.h                 = 1.0;
    shutter_speed         = 10000;// in usec (0 = auto)
    awb_gains_r           = 0;// Only have any function if AWB OFF is used.
    awb_gains_b           = 0;
    drc_level             = MMAL_PARAMETER_DRC_STRENGTH_OFF;
    stats_pass            = MMAL_FALSE;
    enable_annotate       = 0;
    annotate_string[0]    = '\0';
    annotate_text_size    = 0; //Use firmware default
    annotate_text_colour  =-1;//Use firmware default
    annotate_bg_colour    =-1;//Use firmware default
    stereo_mode.mode      = MMAL_STEREOSCOPIC_MODE_NONE;
    stereo_mode.decimate  = MMAL_FALSE;
    stereo_mode.swap_eyes = MMAL_FALSE;
    analog_gain           = 1.0;// Analog gain [1.0 - 12.0]
    digital_gain          = 1.0;// Digital gain [1.0 - 255.0]
    onlyLuma              = MMAL_FALSE;
}


void
MainDialog::dumpParameters() {
    qDebug() << endl;
    qDebug() << "Camera Parameters Dump:";
    qDebug() << endl;
    qDebug() << QString("Camera Name %1")
                .arg(cameraName);
    qDebug() << QString("Width %1, Height %2, filename %3")
                .arg(width)
                .arg(height)
                .arg(filename);
    qDebug() << QString("Using camera %1, sensor mode %2")
                .arg(cameraNum)
                .arg(sensorMode);
    qDebug() << QString("GPS output %1")
                .arg(gps ? "Enabled" : "Disabled");
    qDebug() << endl;
}


MMAL_STATUS_T
MainDialog::setupCameraConfiguration() {
    MMAL_PARAMETER_CAMERA_CONFIG_T camConfig;
    camConfig.hdr = { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(camConfig) };
    camConfig.max_stills_w = uint32_t(width); // Max size of stills capture
    camConfig.max_stills_h = uint32_t(height);
    camConfig.stills_yuv422 = 0;  // Allow YUV422 stills capture
    camConfig.one_shot_stills = 1;// Continuous or one shot stills captures
    if(fullResPreview) {          // Max size of the preview or video capture frames
        camConfig.max_preview_video_w = uint32_t(width);
        camConfig.max_preview_video_h = uint32_t(height);
    }
    else{
        camConfig.max_preview_video_w = uint32_t(pPreview->previewWindow.width);
        camConfig.max_preview_video_h = uint32_t(pPreview->previewWindow.height);
    }
    camConfig.num_preview_video_frames = 3;
    camConfig.stills_capture_circular_buffer_height = 0;// Sets the height of the circular buffer for stills capture
    camConfig.fast_preview_resume = 0;
    camConfig.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;

    MMAL_STATUS_T status = pCamera->setConfig(&camConfig);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Could not set sensor configuration: error") << status;
    }
    return status;
}


void
MainDialog::switchLampOn() {
    if(gpioHostHandle >= 0)
        gpio_write(gpioHostHandle, gpioLEDpin, 1);
    else
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 On")
                              .arg(gpioLEDpin));
    pUi->lampStatus->setStyleSheet(sPhotoStyle);
    repaint();
}


void
MainDialog::switchLampOff() {
    if(gpioHostHandle >= 0)
        gpio_write(gpioHostHandle, gpioLEDpin, 0);
    else
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 Off")
                              .arg(gpioLEDpin));
    pUi->lampStatus->setStyleSheet(sDarkStyle);
    repaint();
}


bool
MainDialog::gpioInit() {
    int iResult;
    gpioHostHandle = pigpio_start(QString("localhost").toLocal8Bit().data(),
                                  QString("8888").toLocal8Bit().data());
    if(gpioHostHandle < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error !"),
                              QString("Non riesco ad inizializzare la GPIO."));
        return false;
    }
    // Led On/Off Control
    iResult = set_mode(gpioHostHandle, gpioLEDpin, PI_OUTPUT);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to initialize GPIO%1 as Output")
                                   .arg(gpioLEDpin));
        return false;
    }

    iResult = set_pull_up_down(gpioHostHandle, gpioLEDpin, PI_PUD_UP);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 Pull-Up")
                                   .arg(gpioLEDpin));
        return false;
    }
    return true;
}


bool
MainDialog::checkValues() {
    QDir dir(sBaseDir);
    if(!dir.exists())
        return false;
    return true;
}


void
MainDialog::getSensorDefaults(int camera_num, char *camera_name, int *width, int *height) {
   MMAL_COMPONENT_T *cameraInfo;
   MMAL_STATUS_T status;
   // Default to the OV5647 setup
   strncpy(camera_name, "OV5647", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
   // Try to get the camera name and maximum supported resolution
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &cameraInfo);
   if(status == MMAL_SUCCESS) {
      MMAL_PARAMETER_CAMERA_INFO_T param;
      param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
      param.hdr.size = sizeof(param)-4;  // Deliberately undersize to check firmware version
      status = mmal_port_parameter_get(cameraInfo->control, &param.hdr);

      if(status != MMAL_SUCCESS) {// Running on newer firmware
         param.hdr.size = sizeof(param);
         status = mmal_port_parameter_get(cameraInfo->control, &param.hdr);
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
            qDebug() << QString("Cannot read camera info, keeping the defaults for OV5647");
      }
      else {
         // Older firmware
         // Nothing to do here, keep the defaults for OV5647
      }
      mmal_component_destroy(cameraInfo);
   }
   else {
      qDebug() << QString("Failed to create camera_info component");
   }
   // default to OV5647 if nothing detected..
   if(*width == 0)
      *width = 2592;
   if(*height == 0)
      *height = 1944;
}


void
MainDialog::on_startButton_clicked() {
    if(!checkValues()) {
        pUi->statusBar->setText((QString("Error: Check Values !")));
        return;
    }
    switchLampOff();
    intervalTimer.start(msecInterval);

    QList<QWidget *> widgets = findChildren<QWidget *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setDisabled(true);
    }
    pUi->stopButton->setEnabled(true);
    pCamera->start(pJpegEncoder);
}


void
MainDialog::on_stopButton_clicked() {
    intervalTimer.stop();
    pCamera->stop(pJpegEncoder);
    switchLampOff();
    QList<QWidget *> widgets = findChildren<QWidget *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
    pUi->stopButton->setDisabled(true);
}


void
MainDialog::on_intervalEdit_textEdited(const QString &arg1) {
    if(arg1.toInt() < MIN_INTERVAL) {
        pUi->intervalEdit->setStyleSheet(sErrorStyle);
    } else {
        msecInterval = arg1.toInt();
        pUi->intervalEdit->setStyleSheet(sNormalStyle);
    }
}


void
MainDialog::on_intervalEdit_editingFinished() {
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval));
    pUi->intervalEdit->setStyleSheet(sNormalStyle);
}


void
MainDialog::on_tTimeEdit_textEdited(const QString &arg1) {
    if(arg1.toInt() < 0) {
        pUi->tTimeEdit->setStyleSheet(sErrorStyle);
    } else {
        secTotTime = arg1.toInt();
        pUi->tTimeEdit->setStyleSheet(sNormalStyle);
    }
}


void
MainDialog::on_tTimeEdit_editingFinished() {
    pUi->tTimeEdit->setText(QString("%1").arg(secTotTime));
    pUi->tTimeEdit->setStyleSheet(sNormalStyle);
}


void
MainDialog::on_pathEdit_textChanged(const QString &arg1) {
    QDir dir(arg1);
    if(!dir.exists()) {
        pUi->pathEdit->setStyleSheet(sErrorStyle);
    }
    else {
        pUi->pathEdit->setStyleSheet(sNormalStyle);
    }
}


void
MainDialog::on_pathEdit_editingFinished() {
    sBaseDir = pUi->pathEdit->text();
}


void
MainDialog::on_nameEdit_textChanged(const QString &arg1) {
    sOutFileName = arg1;
}


//////////////////////////////////////////////////////////////
/// Acquisition timer handler <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//////////////////////////////////////////////////////////////
void
MainDialog::onTimeToGetNewImage() {
    switchLampOn();
    QThread::msleep(10);
    QString sFileName = QString("%1/%2_%3.jpg")
            .arg(sBaseDir)
            .arg(sOutFileName)
            .arg(imageNum, 4, 10, QLatin1Char('0'));
    pCamera->capture(sFileName);
    QThread::msleep(300);
    switchLampOff();
    imageNum++;
}


void
MainDialog::on_aGainSlider_sliderMoved(int position) {
    analog_gain = position/10.0f;
    CameraControl* pCameraControl = pCamera->pControl;
    pCameraControl->set_gains(analog_gain, digital_gain);
    if(verbose)
        qDebug() << __func__ << "New gains=" << analog_gain << digital_gain;
}


void
MainDialog::on_aGainSlider_valueChanged(int value) {
    analog_gain = value/10.0f;
    CameraControl* pCameraControl = pCamera->pControl;
    pCameraControl->set_gains(analog_gain, digital_gain);
    if(verbose)
        qDebug() << __func__ << "New gains=" << analog_gain << digital_gain;
}


void
MainDialog::on_dGainSlider_sliderMoved(int position) {
    digital_gain = position/10.0f;
    CameraControl* pCameraControl = pCamera->pControl;
    pCameraControl->set_gains(analog_gain, digital_gain);
    if(verbose)
        qDebug() << __func__ << "New gains=" << analog_gain << digital_gain;
}


void
MainDialog::on_dGainSlider_valueChanged(int value) {
    digital_gain = value/10.0f;
    CameraControl* pCameraControl = pCamera->pControl;
    pCameraControl->set_gains(analog_gain, digital_gain);
    if(verbose)
        qDebug() << __func__ << "New gains=" << analog_gain << digital_gain;
}


void
MainDialog::on_dialPan_valueChanged(int value) {
    cameraPanValue  = value;
    setPan(cameraPanValue);
    update();
}


void
MainDialog::on_dialTilt_valueChanged(int value) {
    cameraTiltValue = value;
    setTilt(cameraTiltValue);
    update();
}

