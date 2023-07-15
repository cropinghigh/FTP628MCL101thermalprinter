#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QSerialPortInfo>
#include <QThread>
#include <QMessageBox>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QTimer>

#include "serialworker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void loadImageFile(bool ignoreErrors);
    void refreshActivity();

private slots:

    void on_actrxtimer_timeout();

    void on_acttxtimer_timeout();

    void on_updateActivity(bool tx);

    void on_shiftFinished();

    void on_printFinished();

    void onImageChanged(const QString &path);

    void on_updateTimes(long min, long max);

    void on_updateConStatus(SerialWorker::TPN_CONN_STATUS s);

    void on_updatePrintStatus(int printStatus);

    void on_updateVCC(float vcc);

    void on_updateTemp(float temp);

    void gotError(QString err);

    void on_pushButton_5_clicked();

    void on_comboBox_currentTextChanged(const QString &arg1);

    void on_pushButton_clicked();

    void on_pushButton_6_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_4_clicked();

    void on_lineEdit_editingFinished();

    void on_pushButton_2_pressed();

    void on_pushButton_2_released();

    void on_comboBox_2_currentIndexChanged(int index);

private:
    QSettings settings;
    QImage currImage;
    QGraphicsScene* scene;
    QGraphicsPixmapItem* item;
    QFileSystemWatcher imgreloadwatcher;
    QThread processThread;
    Ui::MainWindow *ui;
    QLabel statusbar_conn;
    QLabel statusbar_vcc;
    QLabel statusbar_temp;
    QLabel statusbar_activity;
    SerialWorker::TPN_CONN_STATUS curr_status = SerialWorker::TPN_STATUS_NOTCONNECTED;
    SerialWorker w;
    QMessageBox mb;
    bool shifting = false;
    bool act_tx = false;
    bool act_rx = false;
    QTimer act_tx_tmr;
    QTimer act_rx_tmr;
    QFont font;
};
#endif // MAINWINDOW_H
