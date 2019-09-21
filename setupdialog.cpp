#include "setupdialog.h"
#include "ui_setupdialog.h"
#include <signal.h>
#include "pigpiod_if2.h"// The library for using GPIO pins on Raspberry
#include <QMoveEvent>
#include <QMessageBox>
#include <QSettings>
#include <QThread>
#include <QDebug>


// GPIO Numbers are Broadcom (BCM) numbers
#define PAN_PIN  14 // BCM14 is Pin  8 in the 40 pin GPIO connector.
#define TILT_PIN 26 // BCM26 IS Pin 37 in the 40 pin GPIO connector.


setupDialog::setupDialog(int hostHandle, QWidget *parent)
    : QDialog(parent)
    , pUi(new Ui::setupDialog)
    , pImageRecorder(Q_NULLPTR)
    // ================================================
    // GPIO Numbers are Broadcom (BCM) numbers
    // ================================================
    // +5V on pins 2 or 4 in the 40 pin GPIO connector.
    // GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
    // in the 40 pin GPIO connector.
    // ================================================
    , panPin(PAN_PIN)
    , tiltPin(TILT_PIN)
    , gpioHostHandle(hostHandle)
{
    pUi->setupUi(this);
    setFixedSize(size());

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

    restoreSettings();
}


setupDialog::~setupDialog() {
    if(pImageRecorder) {
        pImageRecorder->terminate();
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        delete pImageRecorder;
        pImageRecorder = nullptr;
    }
    delete pUi;
}


void
setupDialog::closeEvent(QCloseEvent *event) {
    if(event->type() == QCloseEvent::Close) {
        if(pImageRecorder) {
            pImageRecorder->disconnect();
            pImageRecorder->terminate();
            pImageRecorder->close();
            pImageRecorder->waitForFinished(3000);
            delete pImageRecorder;
            pImageRecorder = nullptr;
        }
    }
}


int
setupDialog::exec() {
    pImageRecorder = new QProcess(this);
    connect(pImageRecorder,
            SIGNAL(finished(int, QProcess::ExitStatus)),
            this,
            SLOT(onImageRecorderClosed(int, QProcess::ExitStatus)));
    connect(pImageRecorder,
            SIGNAL(errorOccurred(QProcess::ProcessError)),
            this,
            SLOT(onImageRecorderError(QProcess::ProcessError)));
    connect(pImageRecorder,
            SIGNAL(started()),
            this,
            SLOT(onImageRecorderStarted()));
    return QDialog::exec();
}


void
setupDialog::moveEvent(QMoveEvent *event) {
    if(pImageRecorder) {
        QPoint dialogPos = event->pos();
        QPoint videoPos = pUi->labelVideo->pos();
        QSize videoSize = pUi->labelVideo->size();

        QString sCommand = QString("/usr/bin/raspistill");
        QStringList sArguments = QStringList();
        sArguments.append(QString("-ex auto"));                  // Exposure mode; Auto
        sArguments.append(QString("-awb auto"));                 // White Balance; Auto
        sArguments.append(QString("-drc off"));                  // Dynamic Range Compression: off
        sArguments.append(QString("-vf"));                       // Vertical Flip
        sArguments.append(QString("-md 1"));                     // Mode 1 (1920x1080)
        sArguments.append(QString("-t 0"));
        sArguments.append(QString("-p %1,%2,%3,%4")
                          .arg(dialogPos.x()+videoPos.x())
                          .arg(dialogPos.y()+videoPos.y())
                          .arg(videoSize.width())
                          .arg(videoSize.height()));

////////////////////////////////////////////////////////////
/// Here we could use the following (Not working at present)
//    pImageRecorder->setProgram(sCommand);
//    pImageRecorder->setArguments(sArguments);
//    pImageRecorder->start();
/// Instead we have to use:
        for(int i=0; i<sArguments.size(); i++)
            sCommand += QString(" %1").arg(sArguments[i]);
////////////////////////////////////////////////////////////
        if(pImageRecorder->state() == QProcess::NotRunning)
            pImageRecorder->start(sCommand);
        else {
            pImageRecorder->disconnect();
            pImageRecorder->terminate();
            pImageRecorder->close();
            pImageRecorder->waitForFinished(3000);
            pImageRecorder->start(sCommand);
        }
    }
}


void
setupDialog::restoreSettings() {
    QSettings settings;
    // Restore settings
    cameraPanValue  = settings.value("panValue",  cameraPanValue).toDouble();
    cameraTiltValue = settings.value("tiltValue", cameraTiltValue).toDouble();
    pUi->dialPan->setValue(int(cameraPanValue));
    pUi->dialTilt->setValue(int(cameraTiltValue));
}


bool
setupDialog::panTiltInit() {
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
setupDialog::setPan(double cameraPanValue) {
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
setupDialog::setTilt(double cameraTiltValue) {
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


//////////////////////////////////////////////////////////////
/// Process event handlers <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//////////////////////////////////////////////////////////////
void
setupDialog::on_buttonBox_accepted() {
    // Save settings
    QSettings settings;
    settings.setValue("panValue",  cameraPanValue);
    settings.setValue("tiltValue", cameraTiltValue);
    if(pImageRecorder) {
        pImageRecorder->disconnect();
        pImageRecorder->terminate();
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        delete pImageRecorder;
        pImageRecorder = nullptr;
    }
    accept();
}


void
setupDialog::on_buttonBox_rejected() {
    if(pImageRecorder) {
        pImageRecorder->disconnect();
        pImageRecorder->terminate();
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        delete pImageRecorder;
        pImageRecorder = nullptr;
    }
    reject();
}


void
setupDialog::onImageRecorderStarted() {
    pid = pid_t(pImageRecorder->processId());
}


void
setupDialog::onImageRecorderError(QProcess::ProcessError error) {
    Q_UNUSED(error)
    QMessageBox::critical(this,
                          QString("raspistill"),
                          QString("Error %1")
                          .arg(error));
    QList<QWidget *> widgets = findChildren<QWidget *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
}


void
setupDialog::onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    pImageRecorder->disconnect();
    delete pImageRecorder;
    pImageRecorder = nullptr;
    if(exitCode != 130) {// exitStatus==130 means process killed by Ctrl-C
        QMessageBox::critical(this,
                              QString("raspistill"),
                              QString("exited with status: %1, Exit code: %2")
                              .arg(exitStatus)
                              .arg(exitCode));
    }
    QList<QWidget *> widgets = findChildren<QWidget *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
}


void
setupDialog::on_dialPan_valueChanged(int value) {
    cameraPanValue  = value;
    setPan(cameraPanValue);
    update();
}


void
setupDialog::on_dialTilt_valueChanged(int value) {
    cameraTiltValue = value;
    setTilt(cameraTiltValue);
    update();
}

