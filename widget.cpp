#include "widget.h"
#include "ui_widget.h"
#include <QTimer>
#include <QDebug>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTcpServer>

constexpr int resigtercount = 40;

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

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

            mcuData.clear();
        });
        timer->start(400);
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
    connect(tcpServer, &QTcpServer::acceptError, this, [](QAbstractSocket::SocketError socketError){
        qDebug()<<"socket Error" << socketError;
        qApp->quit();
    });

    if (!tcpServer->listen(QHostAddress::Any, 502)) {
        tcpServer->close(); exit(-1);
    }

    connect(tcpServer, &QTcpServer::newConnection, this, [this, tcpServer]{

        QTcpSocket *clientConnection = tcpServer->nextPendingConnection();
        connect(clientConnection, &QAbstractSocket::disconnected, clientConnection, &QObject::deleteLater);

        connect(clientConnection, &QTcpSocket::readyRead, this, [clientConnection]{
            QByteArray pdu = clientConnection->readAll();
            QByteArray writePdu = pdu.left(8);
            //0x03
            if(1 == pdu[6] && (pdu.size() == 12)) {

                quint16 registerNumber = pdu[10] << 8 | pdu[11];
                writePdu[8] = registerNumber * 2;

                for (int i = 0; i < registerNumber; i++) {
                    writePdu[2 * i + 9] = 0;
                    writePdu[2 * i + 10] = i;
                }
                clientConnection->write(writePdu);
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

    if(values.size() < resigtercount || mcuRegisterValue.size() < resigtercount) {
        return;
    }
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
