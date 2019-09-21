#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QProcess>
#include <sys/types.h>

namespace Ui {
class setupDialog;
}

class setupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit setupDialog(int hostHandle, QWidget *parent = nullptr);
    ~setupDialog();

protected:
    void closeEvent(QCloseEvent *event);
    void restoreSettings();
    bool panTiltInit();
    bool setPan(double cameraPanValue);
    bool setTilt(double cameraTiltValue);

public slots:
    void onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onImageRecorderStarted();
    void onImageRecorderError(QProcess::ProcessError error);
    void on_dialTilt_valueChanged(int value);
    void on_dialPan_valueChanged(int value);
    int  exec();
    void moveEvent(QMoveEvent *event);

private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

private:
    Ui::setupDialog* pUi;
    QProcess*        pImageRecorder;
    pid_t pid;

    uint   panPin;
    uint   tiltPin;
    double cameraPanValue;
    double cameraTiltValue;
    uint   PWMfrequency;     // in Hz
    int    pulseWidthAt_90;  // in us
    int    pulseWidthAt90;   // in us
    int    gpioHostHandle;
};

#endif // SETUPDIALOG_H
