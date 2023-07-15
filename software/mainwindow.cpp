#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), settings("Indir", "thermoprinter2"), font("Monospace") {
    ui->setupUi(this);
    font.setStyleHint(QFont::TypeWriter);
    font.setPointSize(15);
    font.setBold(true);
    statusbar_conn.setFont(font);
    statusbar_vcc.setFont(font);
    statusbar_temp.setFont(font);
    statusbar_activity.setFont(font);
    ui->statusbar->setFont(font);
    ui->statusbar->addPermanentWidget(&statusbar_conn);
    ui->statusbar->addPermanentWidget(&statusbar_vcc);
    ui->statusbar->addPermanentWidget(&statusbar_temp);
    ui->statusbar->addPermanentWidget(&statusbar_activity);
    ui->statusbar->clearMessage();
    QObject::connect(&w, &SerialWorker::updateConStatus, this, &MainWindow::on_updateConStatus, Qt::QueuedConnection);
    QObject::connect(&w, &SerialWorker::updatePrintStatus, this, &MainWindow::on_updatePrintStatus, Qt::QueuedConnection);
    QObject::connect(&w, &SerialWorker::printFinished, this, &MainWindow::on_printFinished, Qt::QueuedConnection);
    QObject::connect(&w, &SerialWorker::shiftFinished, this, &MainWindow::on_shiftFinished, Qt::QueuedConnection);
    QObject::connect(&w, &SerialWorker::updateActivity, this, &MainWindow::on_updateActivity, Qt::QueuedConnection);
    QObject::connect(&w, &SerialWorker::updateVCC, this, &MainWindow::on_updateVCC, Qt::QueuedConnection);
    QObject::connect(&w, &SerialWorker::updateTemp, this, &MainWindow::on_updateTemp, Qt::QueuedConnection);
    QObject::connect(&w, &SerialWorker::gotError, this, &MainWindow::gotError, Qt::QueuedConnection);
    QObject::connect(&w, &SerialWorker::updateTimes, this, &MainWindow::on_updateTimes, Qt::QueuedConnection);
    QObject::connect(&act_rx_tmr, &QTimer::timeout, this, &MainWindow::on_actrxtimer_timeout, Qt::QueuedConnection);
    QObject::connect(&act_tx_tmr, &QTimer::timeout, this, &MainWindow::on_acttxtimer_timeout, Qt::QueuedConnection);
    on_pushButton_5_clicked();
    statusbar_conn.setText("Disconected");

    w.moveToThread(&processThread);
    QObject::connect(&processThread, SIGNAL(started()), &w, SLOT(mainLoop()), Qt::DirectConnection);
    QObject::connect(&w, SIGNAL(threadFinished()), &processThread, SLOT(quit()), Qt::DirectConnection);

    QObject::connect(&imgreloadwatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onImageChanged);
    QString prevfile = settings.value("prevfile").toString();
    if(prevfile != "") {
        ui->lineEdit->setText(prevfile);
        loadImageFile(false);
        ui->comboBox_2->setEnabled(true);
    }
    ui->comboBox_2->setCurrentIndex(settings.value("prevqual", 0).toInt());
    refreshActivity();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::loadImageFile(bool ignoreErrors) {
    if(ui->lineEdit->text() != "") {
        if(currImage.load(ui->lineEdit->text())) {
            imgreloadwatcher.removePaths(imgreloadwatcher.files());
            imgreloadwatcher.addPath(ui->lineEdit->text());
            settings.setValue("prevfile", ui->lineEdit->text());
            currImage = currImage.convertToFormat(QImage::Format_Grayscale8).scaledToWidth(384, Qt::SmoothTransformation);
            if(currImage.width() != 384) {
                gotError("Wrong width!");
            }
            //Convert data to 0-15/0-7/0-3/0-1 from 0-255
            int colorcnt = pow(2, (4-ui->comboBox_2->currentIndex()));
            int divider = 256/(colorcnt-1);
            int halfdivider = divider/2;
            for(int x = 0; x < currImage.height(); x++) {
                uchar *line = currImage.scanLine(x);
                for(int y = 0; y < currImage.width(); y++) {
                    uint16_t prev_val = (uint16_t) line[y];
                    uint16_t div_val = prev_val + halfdivider;
                    uint16_t divided_val = div_val / divider;
                    uint16_t mul = divided_val * divider;
                    uchar new_val = mul;
                    if(mul > 255) {
                        new_val = 255;
                    }
                    line[y] = new_val;
                }
            }
            scene = new QGraphicsScene();
            ui->graphicsView_2->setScene(scene);
            item = new QGraphicsPixmapItem(QPixmap::fromImage(currImage));
            scene->addItem(item);
            ui->graphicsView_2->show();
            if(curr_status == SerialWorker::TPN_STATUS_CONNECTED) {
                ui->pushButton_4->setEnabled(true);
            }
        } else {
            if(!ignoreErrors) {
                gotError("Can't load image!");
            }
        }
    }
}

void MainWindow::refreshActivity() {
    statusbar_activity.setText(QString("<pre><span style='color: #FF0000;'>") + QString(act_tx ? "↑" : " ") + QString("</span><span style='color: #00FF00;'>") + QString(act_rx ? "↓" : " ") + QString("</span></pre>"));
}

void MainWindow::onImageChanged(const QString &path) {
    loadImageFile(true);
}

void MainWindow::on_updateTimes(long min, long max) {
    ui->spinBox->setValue(max);
    ui->spinBox_2->setValue(min);
}

void MainWindow::on_updateConStatus(SerialWorker::TPN_CONN_STATUS s) {
    curr_status = s;
    if(s == SerialWorker::TPN_STATUS_NOTCONNECTED) {
        ui->pushButton->setEnabled(true);
        ui->pushButton->setText("Connect");
        ui->pushButton_5->setEnabled(true);
        ui->comboBox->setEnabled(true);
        ui->pushButton_2->setEnabled(false);
        ui->pushButton_3->setEnabled(true);
        ui->lineEdit->setEnabled(true);
        ui->pushButton_4->setEnabled(false);
        ui->pushButton_6->setEnabled(false);
        ui->spinBox->setEnabled(false);
        ui->spinBox_2->setEnabled(false);
        ui->statusbar->clearMessage();
        shifting = false;
        act_tx = false;
        act_rx = false;
        ui->pushButton_4->setText("Print");
        if(!currImage.isNull()) {
            item->setPixmap(QPixmap::fromImage(currImage));
            ui->comboBox_2->setEnabled(true);
        }
        statusbar_conn.setText("Disconnected");
        refreshActivity();
        statusbar_temp.setText("");
        statusbar_vcc.setText("");
    } else if(s == SerialWorker::TPN_STATUS_CONNECTING) {
        ui->pushButton->setText("Stop");
        ui->pushButton_5->setEnabled(false);
        ui->comboBox->setEnabled(false);
        ui->pushButton_2->setEnabled(false);
        ui->pushButton_3->setEnabled(false);
        ui->lineEdit->setEnabled(false);
        ui->comboBox_2->setEnabled(false);
        ui->pushButton_4->setEnabled(false);
        ui->pushButton_6->setEnabled(false);
        ui->spinBox->setEnabled(false);
        ui->spinBox_2->setEnabled(false);
        ui->statusbar->clearMessage();
        statusbar_conn.setText("Connecting...");
    } else {
        ui->pushButton->setText("Disconnect");
        ui->pushButton_5->setEnabled(false);
        ui->comboBox->setEnabled(false);
        ui->pushButton_2->setEnabled(true);
        ui->pushButton_3->setEnabled(true);
        ui->lineEdit->setEnabled(true);
        if(!currImage.isNull()) {
            ui->pushButton_4->setEnabled(true);
            ui->pushButton_4->setText("Print");
            ui->comboBox_2->setEnabled(true);
        }
        ui->pushButton_6->setEnabled(true);
        ui->spinBox->setEnabled(true);
        ui->spinBox_2->setEnabled(true);
        ui->statusbar->clearMessage();
        statusbar_conn.setText("Connected");
    }
}

void MainWindow::on_actrxtimer_timeout() {
    act_rx = false;
    refreshActivity();
}

void MainWindow::on_acttxtimer_timeout() {
    act_tx = false;
    refreshActivity();
}

void MainWindow::on_updateActivity(bool tx) {
    if(tx) {
        act_tx = true;
        act_tx_tmr.start(20);
    } else {
        act_rx = true;
        act_rx_tmr.start(20);
    }
    refreshActivity();
}

void MainWindow::on_shiftFinished() {
    if(shifting) {
        w.shift();
    }
}

void MainWindow::on_printFinished() {
    if(!currImage.isNull()) {
        ui->pushButton_4->setText("Print");
        ui->comboBox_2->setEnabled(true);
    }
    item->setPixmap(QPixmap::fromImage(currImage));
    ui->statusbar->clearMessage();
    ui->pushButton->setEnabled(true);
    ui->pushButton_2->setEnabled(true);
    ui->pushButton_3->setEnabled(true);
    ui->lineEdit->setEnabled(true);
    ui->pushButton_6->setEnabled(true);
}

void MainWindow::on_updatePrintStatus(int printStatus) {
    ui->statusbar->showMessage("Printing process: " + QString::number(printStatus) + "/" + QString::number(currImage.height()));
    QImage displayImage = currImage;
    for(int i = 0; i < 384; i++) {
        if(printStatus > 1) {
            displayImage.scanLine(printStatus-2)[i] = 0;
        }
        displayImage.scanLine(printStatus-1)[i] = 0;
        if(printStatus < currImage.height()) {
            displayImage.scanLine(printStatus)[i] = 0;
        }
    }
    item->setPixmap(QPixmap::fromImage(displayImage));
}

void MainWindow::on_updateVCC(float vcc) {
    statusbar_vcc.setText("V: " + QString::number(vcc) + "V  ");
}

void MainWindow::on_updateTemp(float temp) {
    statusbar_temp.setText("T: " + QString::number(temp) + "°C  ");
}

void MainWindow::gotError(QString err) {
    mb.critical(this, "ERROR!", "Error!\n" + err);
}

void MainWindow::on_pushButton_5_clicked() {
    QList<QSerialPortInfo> aports = QSerialPortInfo::availablePorts();
    ui->comboBox->clear();
    QString prevselected = settings.value("prevport").toString();
    for(int i = 0; i < aports.size(); i++) {
        ui->comboBox->addItem(aports[i].portName());
        if(aports[i].portName() == prevselected) {
            ui->comboBox->setCurrentIndex(i);
        }
    }
}


void MainWindow::on_comboBox_currentTextChanged(const QString &arg1) {
    ui->pushButton->setEnabled(true);
}


void MainWindow::on_pushButton_clicked() {
    if(curr_status == SerialWorker::TPN_STATUS_NOTCONNECTED) {
        QString port_name = ui->comboBox->currentText();
        settings.setValue("prevport", port_name);
        w.setPort(port_name);
        processThread.start();
    } else {
        w.requestDisconnect();
    }
}


void MainWindow::on_pushButton_6_clicked() {
    w.setTimes(ui->spinBox_2->value(), ui->spinBox->value());
}


void MainWindow::on_pushButton_2_clicked() {
    w.shift();
}


void MainWindow::on_pushButton_3_clicked() {
    QString prevfile = settings.value("prevfile", "/home/123.png").toString();
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image"), QFileInfo(prevfile).absoluteDir().absolutePath() , tr("Images (*.png *.xpm *.jpg *.jpeg *.tiff *.gif)"));
    if(fileName != "") {
        ui->lineEdit->setText(fileName);
        loadImageFile(false);
    }
}


void MainWindow::on_pushButton_4_clicked() {
    if(ui->pushButton_4->text() == "Print") {
        w.print(currImage);
        ui->pushButton_4->setText("Break print");
        ui->pushButton->setEnabled(false);
        ui->pushButton_2->setEnabled(false);
        ui->pushButton_3->setEnabled(false);
        ui->lineEdit->setEnabled(false);
        ui->pushButton_6->setEnabled(false);
        ui->comboBox_2->setEnabled(false);
    } else {
        w.breakPrint();
    }
}


void MainWindow::on_lineEdit_editingFinished() {
    loadImageFile(false);
}


void MainWindow::on_pushButton_2_pressed() {
    shifting = true;
    w.shift();
}


void MainWindow::on_pushButton_2_released() {
    shifting = false;
}


void MainWindow::on_comboBox_2_currentIndexChanged(int index) {
    settings.setValue("prevqual", index);
    loadImageFile(false);
}

