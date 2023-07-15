#include "serialworker.h"

SerialWorker::SerialWorker() {

}

SerialWorker::~SerialWorker() {
    requestDisconnect();
}

void SerialWorker::writeSerial(QString msg) {
    emit updateActivity(true);
    if(write(serial_fd, msg.toUtf8().constData(), msg.length()) < 0) {
        disconnect();
        emit gotError("Write failed");
    }
}

QString SerialWorker::blockingReadString() {
    if(readBuffer.empty()) {
        readAndQueueLines();
    }
    if(!readBuffer.empty()) {
        QString output = readBuffer.front();
        readBuffer.pop();
        return output;
    } else {
        //timeout
        return "";
    }

}

void SerialWorker::readAndQueueLines() {
    bool working = true;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    QString output = "";
    bool processed = false;
    while(working && serial_fd > 0) {
        char c_arr[512];
        int cnt = read(serial_fd, c_arr, sizeof(c_arr));
        if(cnt > 0) {
            for(int i = 0; i < cnt; i++) {
//                    if(c_arr[i] == '\r') {
//                        continue;
//                    }
                if(c_arr[i] == '\n') {
                    //string read finished
                    readBuffer.push(output);
                    output = "";
                    processed = true;
                    emit updateActivity(false);
                    continue;
                }
                output += c_arr[i];
            }
        } else {
            if(processed) {
                if(output != "") {
                    readBuffer.push(output);
                }
                return;
            }
            thread()->msleep(5);
            float time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if(time >= 2000) {
                working = false;
            }
        }
    }
    if(output != "") {
        readBuffer.push(output);
    }
}

void SerialWorker::setPort(QString portName) {
    portname = portName;
}

void SerialWorker::disconnect() {
    if(serial_fd > 0) {
        writeSerial("msd\n");
        close(serial_fd);
        serial_fd = -1;
    }
    std::queue<QString> empty;
    std::swap(readBuffer, empty); //clear
    emit updateConStatus(TPN_STATUS_NOTCONNECTED);
    curr_link_status = 0;
    print_active = false;
}

void SerialWorker::requestDisconnect() {
    print_active = false;
    thread()->requestInterruption();
    thread()->wait(2000);
    thread()->terminate();
    if(serial_fd > 0) {
        close(serial_fd);
        serial_fd = -1;
    }
}

void SerialWorker::print(QImage im) {
    if(print_request || !printingimg.isNull()) {
        return;
    }
    printingimg = im;
    print_request = true;
}

void SerialWorker::breakPrint() {
    if(print_request || printingimg.isNull()) {
        return;
    }
    print_request = true;
}

void SerialWorker::shift() {
    if(serial_fd < 0) {
        return;
    }
    shift_request++;
}

void SerialWorker::setTimes(long min, long max) {
    if(serial_fd < 0) {
        return;
    }
    newmintime = min;
    newmaxtime = max;
    times_set_request = true;
}

void SerialWorker::mainLoop() {
    if(serial_fd > 0) {
        close(serial_fd);
        serial_fd = -1;
    }
    serial_fd = open(("/dev/" + portname).toUtf8().constData(), O_RDWR);
    if(serial_fd < 0) {
        emit gotError(QString::fromUtf8(strerror(errno)));
        disconnect();
        return;
    }
    emit updateConStatus(TPN_STATUS_CONNECTING);
    struct termios tty;
    if(tcgetattr(serial_fd, &tty) != 0) {
        emit gotError(QString::fromUtf8(strerror(errno)));
        disconnect();
        return;
    }
    tty.c_cflag &= ~PARENB; // Clear parity bit
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used
    tty.c_cflag &= ~CSIZE; // Clear all the size bits
    tty.c_cflag |= CS8; // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines
    tty.c_lflag = 0; // Disable canonical mode
    tty.c_iflag = 0; // Turn off s/w flow ctrl & Disable any special handling of received bytes
    tty.c_oflag = 0; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;
    // Set baud rate
    cfsetspeed(&tty, B500000);
    if(tcsetattr(serial_fd, TCSANOW, &tty)) {
        emit gotError(QString::fromUtf8(strerror(errno)));
        disconnect();
        return;
    }
    curr_link_status = 0;
    timer = std::chrono::steady_clock::now();
    while(!thread()->isInterruptionRequested() && serial_fd > 0) {
        switch(curr_link_status) {
            case 0: {
                //Not setup
                int rdbytes = 0;
                if(ioctl(serial_fd, FIONREAD, &rdbytes) < 0) {
                    disconnect();
                    emit gotError(QString::fromUtf8(strerror(errno)));
                }
                if(rdbytes > 0) {
                    QString line = blockingReadString().trimmed();
                    if(line.contains("setup")) {
                        writeSerial("ok\n");
                    } else if(line.contains("ok")) {
                        curr_link_status = 2;
                        emit updateConStatus(TPN_STATUS_CONNECTED);
                        timer = std::chrono::steady_clock::now();
                        timer2 = std::chrono::steady_clock::now();
                    }
                } else {
                    thread()->msleep(100);
                    float time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timer).count();
                    if(time >= 7000) {
                        //timeout!
                        disconnect();
                        emit gotError("Setup timeout(check selected port or device power)");
                    }
                }
                break;
            }
            case 1: {
                //Setup finished, idle
                if(times_set_request) {
                    curr_link_status = 3;
                    times_set_request = false;
                    break;
                }
                if(shift_request > 0) {
                    curr_link_status = 4;
                    shift_request--;
                    break;
                }
                if(motor_en) {
                    float time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timer).count();
                    //Disable motor after 2 seconds of idling to save power
                    if(time >= 2000) {
                        writeSerial("msd\n");
                        QString line = blockingReadString().trimmed();
                        if(line != "f") {
                            disconnect();
                            emit gotError("No response(msd)");
                            break;
                        }
                        motor_en = false;
                    }
                }
                float time_meas = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timer2).count();
                if(time_meas >= 1000) {
                    //Measure VCC and temp every second
                    curr_link_status = 5;
                    timer2 = std::chrono::steady_clock::now();
                    break;
                }
                if(print_request) {
                    curr_print_x = 0;
                    curr_link_status = 6;
                    print_request = false;
                    print_active = true;
                    break;
                }
                if(print_active) {
                    curr_link_status = 6;
                    break;
                }
                thread()->msleep(50);
                break;
            }
            case 2: {
                //Getting times
                bool ok = false;
                long minTime;
                long maxTime;
                writeSerial("getMinTime\n");
                QString line = blockingReadString().trimmed();
                if(line == "unknown") {
                    disconnect();
                    emit gotError("Command error(getmintime)");
                    break;
                } else {
                    minTime = line.toLong(&ok, 10);
                }
                if(!ok) {
                    disconnect();
                    emit gotError("Corrupt data(getmintime, " + line + ")");
                    break;
                }
                line = blockingReadString().trimmed(); //finished
                if(line != "f") {
                    disconnect();
                    emit gotError("No response(getmintime)");
                    break;
                }
                writeSerial("getMaxTime\n");
                line = blockingReadString().trimmed();
                if(line == "unknown") {
                    disconnect();
                    emit gotError("Command error(getmaxtime)");
                    break;
                } else {
                    maxTime = line.toLong(&ok, 10);
                }
                if(!ok) {
                    disconnect();
                    emit gotError("Corrupt data(getmaxtime, " + line + ")");
                    break;
                }
                line = blockingReadString().trimmed(); //finished
                if(line != "f") {
                    disconnect();
                    emit gotError("No response(getmaxtime)");
                    break;
                }
                emit updateTimes(minTime, maxTime);
                curr_link_status = 1;
                break;
            }
            case 3: {
                //Setting times
                writeSerial("setMinTime\n");
                QString line = blockingReadString().trimmed();
                if(line != "r") {
                    disconnect();
                    emit gotError("Command error(setmintime, " + line + ")");
                    break;
                }
                writeSerial(QString::number(newmintime) + "\n");
                line = blockingReadString().trimmed(); //finished
                if(line != "f") {
                    disconnect();
                    emit gotError("No response(setmintime)");
                    break;
                }
                writeSerial("setMaxTime\n");
                line = blockingReadString().trimmed();
                if(line != "r") {
                    disconnect();
                    emit gotError("Command error(setmintime, " + line + ")");
                    break;
                }
                writeSerial(QString::number(newmaxtime) + "\n");
                line = blockingReadString().trimmed(); //finished
                if(line != "f") {
                    disconnect();
                    emit gotError("No response(setmintime)");
                    break;
                }
                curr_link_status = 2;
                break;
            }
            case 4: {
                //Explicit shift
                for(int i = 0; i < 4; i++) {
                    writeSerial("shift\n");
                    QString line = blockingReadString().trimmed();
                    if(line != "f") {
                        disconnect();
                        emit gotError("No response(shift)");
                        break;
                    }
                }
                motor_en = true;
                timer = std::chrono::steady_clock::now();
                curr_link_status = 1;
                emit shiftFinished();
                break;
            }
            case 5: {
                //Read VCC and temp
                bool ok = false;
                float temp;
                writeSerial("getTemp\n");
                QString line = blockingReadString().trimmed();
                if(line == "unknown") {
                    disconnect();
                    emit gotError("Command error(gettemp)");
                    break;
                } else {
                    temp = line.toFloat(&ok);
                }
                if(!ok) {
                    disconnect();
                    emit gotError("Corrupt data(gettemp, " + line + ")");
                    break;
                }
                line = blockingReadString().trimmed(); //finished
                if(line != "f") {
                    disconnect();
                    emit gotError("No response(gettemp)");
                    break;
                }
                emit updateTemp(temp);
                float vcc;
                writeSerial("getVCC\n");
                line = blockingReadString().trimmed();
                if(line == "unknown") {
                    disconnect();
                    emit gotError("Command error(getvcc)");
                    break;
                } else {
                    vcc = line.toFloat(&ok);
                }
                if(!ok) {
                    disconnect();
                    emit gotError("Corrupt data(getvcc, " + line + ")");
                    break;
                }
                line = blockingReadString().trimmed(); //finished
                if(line != "f") {
                    disconnect();
                    emit gotError("No response(getvcc)");
                    break;
                }
                emit updateVCC(vcc);
                curr_link_status = 1;
                break;
            }
            case 6: {
                //Printing!
                if(print_request) {
                    //break
                    print_request = false;
                    print_active = false;
                    emit printFinished();
                    QImage nul;
                    printingimg.swap(nul);
                    curr_link_status = 1;
                    motor_en = true;
                    timer = std::chrono::steady_clock::now();
                    break;
                }
                writeSerial("print\n");
                int curr_y = 0;
                for(int i = 0; i < 6; i++) {
                    QString line = blockingReadString().trimmed();
                    if(!line.startsWith("r")) {
                        disconnect();
                        emit gotError("Command error(print)");
                        break;
                    }
                    QString linedata;
                    for(int y = 0; y < 64; y++) {
                        uint16_t inval = printingimg.scanLine(curr_print_x)[curr_y+y];
                        char val = '?' - ((inval)/16); //invert color
                        linedata += val;
                    }
                    curr_y+=64;
                    writeSerial(linedata + "\n");
                }
                QString line = blockingReadString().trimmed(); //finished
                if(line == "OVERHEAT!") {
                    emit gotError("OVERHEAT!");
                    line = blockingReadString().trimmed(); //finished
                }
                if(line != "f") {
                    disconnect();
                    emit gotError("No response(print, " + line + ")");
                    break;
                }
                curr_print_x++;
                if(curr_print_x > printingimg.height()) {
                    print_active = false;
                    emit printFinished();
                    QImage nul;
                    printingimg.swap(nul);
                    curr_link_status = 1;
                    motor_en = true;
                    timer = std::chrono::steady_clock::now();
                    break;
                }
                emit updatePrintStatus(curr_print_x);
                curr_link_status = 1;
                break;
            }
            default: {
                //?
                disconnect();
                emit gotError("wtf" + QString::number(curr_link_status));
                return;
            }
        }
    }
    disconnect();
    emit threadFinished();
}
