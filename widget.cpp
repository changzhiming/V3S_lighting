#include "widget.h"
#include "ui_widget.h"
#include <QTimer>
#include <QDebug>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTcpServer>
#include <QSettings>
#include <QTime>
#include <crc.h>

constexpr int resigtercount = 40;

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget), mcuRegisterValue(40, 65535)
{
    ui->setupUi(this);

    QSettings iniSetting("uartsetting.ini", QSettings::IniFormat);
    deviceAddress = iniSetting.value("deviceAddress").toInt();

    //! transmit uart

    QSerialPort *transmit= new QSerialPort("ttyS1", this);
    transmit->setBaudRate(iniSetting.value("BaudRate").toInt());
    qDebug()<<iniSetting.value("BaudRate").toInt();
    transmit->setParity(QSerialPort::NoParity);
    transmit->setDataBits(QSerialPort::Data8);
    transmit->setStopBits(QSerialPort::OneStop);

    if(transmit->open(QSerialPort::ReadWrite)) {
        static QTime elapsedTime;
        connect(transmit, &QSerialPort::readyRead, this, [this, transmit]{
            recive_485.append(transmit->readAll());
            elapsedTime.start();
        });

        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]{
            if(elapsedTime.elapsed() > 50 && recive_485.length() > 4) {
                //tcp send
                if(modbustcp) {
                    if(fCrc16(recive_485.data(), recive_485.length()) == 0) {
                        modbustcp->write(pduHeader + recive_485.left(recive_485.length() - 2));
                    }
                }
                recive_485.clear();
            }
        });
        timer->start(5);
    } else {
        qDebug()<<"485 seial open fail reboot";
    }

    //! mcu communication
    QSerialPort *mcuCommunication = new QSerialPort("ttyS0", this);
    mcuCommunication->setBaudRate(115200);
    mcuCommunication->setParity(QSerialPort::NoParity);
    mcuCommunication->setDataBits(QSerialPort::Data8);
    mcuCommunication->setStopBits(QSerialPort::OneStop);

    if(mcuCommunication->open(QSerialPort::ReadWrite)) {

        connect(mcuCommunication, &QSerialPort::readyRead, this, [this, mcuCommunication]{
            mcuData.append(mcuCommunication->readAll());
            if(mcuData.size() == resigtercount * 2) {
                mcuDataChandle(mcuData);
            }
        });

        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]{
            writeDataToMcu(QByteArray().append(0x03).append(resigtercount));

            //test
            if(mcuData.length() != 80) {
                qDebug()<<mcuData.toHex();
            }
            mcuData.clear();
        });
        timer->start(500);
    } else {
        qDebug()<<"mcu seial open fail reboot";
    }

    //! 定时间隔发送指令
    QTimer *writeDataInterval = new QTimer(this);
    connect(writeDataInterval, &QTimer::timeout, this, [this, mcuCommunication]{

        if(!writeDataList.isEmpty()) {
            mcuCommunication->write(writeDataList.takeFirst());
            mcuCommunication->waitForBytesWritten(200);
        }
    });
    writeDataInterval->start(200);

    //! modbus tcp

    QTcpServer * tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::acceptError, this, [](QAbstractSocket::SocketError socketError){qApp->quit();});

    if (!tcpServer->listen(QHostAddress::Any, 502)) {tcpServer->close(); exit(-1);}

    connect(tcpServer, &QTcpServer::newConnection, this, [transmit, this, tcpServer]{

        modbustcp = tcpServer->nextPendingConnection(); // current tcp
        connect(modbustcp, &QAbstractSocket::disconnected, [this]{ modbustcp->deleteLater(); modbustcp = nullptr; });

        connect(modbustcp, &QTcpSocket::readyRead, this, [transmit, this]{
            QByteArray pdu = modbustcp->readAll();

            // transmit 485
            if(pdu.length() >= 12) {

                if(deviceAddress == pdu[6]) {

                    quint16 startAddress = pdu[8] << 8 | pdu[9];
                    //write
                    if(startAddress < 40 && 0x06 == pdu[7]) {
                        quint16 writeValue = pdu[10] << 8 | pdu[11];
                        writeDataToMcu(startAddress, writeValue);
                        modbustcp->write(pdu);
                    }

                    quint16 registerNumber = pdu[10] << 8 | pdu[11];
                    if((startAddress + registerNumber) <= 40 && 0x03 == pdu[7]) {
                        QByteArray writePdu = pdu.left(8);
                        writePdu[8] = registerNumber * 2;

                        for (int i = startAddress; i < startAddress + registerNumber; i++) {
                            writePdu[2 * i + 9] = mcuRegisterValue[i] >> 8;
                            writePdu[2 * i + 10] = mcuRegisterValue[i] & 0xFF;
                        }
                        modbustcp->write(writePdu);
                    }

                } else {
                    pduHeader = pdu.left(6);
                    QByteArray send485data = pdu.right(pdu.length() - 6);
                    quint16 crc = fCrc16(send485data.data(), send485data.length());
                    send485data[send485data.length()] = crc & 0xFF;
                    send485data[send485data.length()] = crc >> 8;

                    //485 send
                    if(deviceAddress != send485data[0]) {
                        transmit->write(send485data);
                        transmit->waitForBytesWritten(200);
                    }
                }
            }
        });
    });
}

void Widget::mcuDataChandle(const QByteArray &data)
{
    if(data.size() % 2 != 0) {
        return;
    }

    QVector<quint16> values;
    for (int index = 0; index <= data.size() - 2; index += 2) {
        values.append(*(quint16 *)data.mid(index, 2).data());
    }

    if(values.size() < 40 || mcuRegisterValue.size() < 40) {
        return;
    }

    QString styleSheetChecked = "QLabel{ background-color: red; border-radius: 15px;}";
    QString styleSheetNoChecked = "QLabel{border-radius: 15px;border-style: solid;border-width: 2px;border-color: gray;}";

    bool requirePublishData = false;

    for(int i = 0; i < values.size(); i++) {
        if(i == 0 && values[i] != mcuRegisterValue[i]) {
//            ui->label_5->setStyleSheet(values[i] ? styleSheetChecked : styleSheetNoChecked);
//            ui->label_7->setStyleSheet(!values[i] ? styleSheetChecked : styleSheetNoChecked);
        }
    }

    mcuRegisterValue = values;
}

void Widget::writeDataToMcu(const QByteArray &data)
{
    if(writeDataList.length() < 1000) {
        writeDataList.append(data);
    }
}

void Widget::writeDataToMcu(int portNumber, quint16 orderValue)
{
    return writeDataToMcu(QByteArray().append(0x04).append((char)portNumber).append((char *)&orderValue, 2));
}

Widget::~Widget()
{
    delete ui;
}
