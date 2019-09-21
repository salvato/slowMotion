#include <QDebug>
#include "maindialog.h"
#include "ui_maindialog.h"
#include "pigpiod_if2.h"// The library for using GPIO pins on Raspberry
#include "setupdialog.h"
#include <QThread>
#include <QMoveEvent>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include <QDir>


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


MainDialog::MainDialog(QWidget *parent)
    : QDialog(parent)
    , pUi(new Ui::MainDialog)
    , gpioLEDpin(LED_PIN)
    , gpioHostHandle(-1)
{
    MMAL_STATUS_T status;

    pUi->setupUi(this);
    setFixedSize(size());

    dialogPos = pos();
    videoPos  = pUi->labelVideo->pos();
    videoSize = pUi->labelVideo->size();

    // Setup the QLineEdit styles
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
    if(!gpioInit())
        exit(EXIT_FAILURE);

    pSetupDlg = new setupDialog(gpioHostHandle);

    switchLampOff();

    restoreSettings();

    // Init User Interface with restored values
    pUi->pathEdit->setText(sBaseDir);
    pUi->nameEdit->setText(sOutFileName);
    pUi->startButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval));
    pUi->tTimeEdit->setText(QString("%1").arg(secTotTime));
    pUi->labelVideo->setStyleSheet(sBlackStyle);

    intervalTimer.stop();// Probably non needed but...does'nt hurt
    connect(&intervalTimer,
            SIGNAL(timeout()),
            this,
            SLOT(onTimeToGetNewImage()));

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
    pCameraControl->set_flips(pCamera->cameraComponent, 0, 1);
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
    videoPos = pUi->labelVideo->pos();
    videoSize = pUi->labelVideo->size();
// TODO: move the Preview window !!
}


void
MainDialog::restoreSettings() {
    QSettings settings;
    // Restore settings
    sBaseDir        = settings.value("BaseDir",
                                     QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    sOutFileName    = settings.value("FileName",
                                     QString("test")).toString();
    msecInterval    = settings.value("Interval", 10000).toInt();
    secTotTime      = settings.value("TotalTime", 0).toInt();

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
            qDebug() << QString("Cannot read camera info, keeping the defaults for OV5647");
      }
      else {
         // Older firmware
         // Nothing to do here, keep the defaults for OV5647
      }
      mmal_component_destroy(camera_info);
   }
   else {
      qDebug() << QString("Failed to create camera_info component");
   }
   // default to OV5647 if nothing detected..
   if (*width == 0)
      *width = 2592;
   if (*height == 0)
      *height = 1944;
}



void
MainDialog::on_startButton_clicked() {
    if(!checkValues()) {
        pUi->statusBar->setText((QString("Error: Check Values !")));
        return;
    }
    imageNum = 0;

//TODO:
    intervalTimer.start(msecInterval);

    QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setDisabled(true);
    }
    pUi->setupButton->setDisabled(true);
    pUi->startButton->setDisabled(true);
    pUi->stopButton->setEnabled(true);
    pCamera->start(pPreview);
}


void
MainDialog::on_stopButton_clicked() {
    intervalTimer.stop();
    pCamera->stop();
    switchLampOff();
    QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
    pUi->startButton->setEnabled(true);
    pUi->setupButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
}


void
MainDialog::on_setupButton_clicked() {
    pSetupDlg->exec();
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

// TODO:
    QThread::msleep(300);
    switchLampOff();
}

