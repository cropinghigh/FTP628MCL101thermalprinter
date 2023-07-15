#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QThread>
#include <QImage>

#include <chrono>
#include <queue>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

class SerialWorker : public QObject {
    Q_OBJECT
public:
    enum TPN_CONN_STATUS {
        TPN_STATUS_NOTCONNECTED,
        TPN_STATUS_CONNECTING,
        TPN_STATUS_CONNECTED
    };

    SerialWorker();
    ~SerialWorker();
private:
    void writeSerial(QString msg);
    QString blockingReadString();
    void readAndQueueLines();
    void disconnect();

    std::queue<QString> readBuffer;
    int serial_fd = -1;
    int curr_link_status = 0;
    bool times_set_request = false;
    long newmintime, newmaxtime;
    int shift_request = 0;
    bool print_request = false;
    bool print_active = false;
    QImage printingimg;
    int curr_print_x = 0;
    std::chrono::steady_clock::time_point timer;
    std::chrono::steady_clock::time_point timer2;
    bool motor_en = false;
    QString portname;

public slots:
    void setPort(QString portName);
    void requestDisconnect();
    void print(QImage im);
    void breakPrint();
    void shift();
    void setTimes(long min, long max);
    void mainLoop();

signals:
    void updateConStatus(TPN_CONN_STATUS s);
    void updatePrintStatus(int printStatus);
    void printFinished();
    void shiftFinished();
    void updateActivity(bool tx);
    void updateVCC(float vcc);
    void updateTemp(float temp);
    void updateTimes(long min, long max);
    void gotError(QString err);
    void threadFinished();
};

#endif // SERIALWORKER_H
