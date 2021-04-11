#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QSerialPort>
#include <QTcpSocket>
#include <QPointer>

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

private:
    void mcuDataChandle(const QByteArray &data);
    void writeDataToMcu(const QByteArray &data);
    void writeDataToMcu(int portNumber, quint16 orderValue);

private:
    Ui::Widget *ui;
    //485
    QByteArray mcuData;  //查询03数据包
    QByteArray recive_485; //
    QByteArray pduHeader;

    QVector<quint16> mcuRegisterValue;
    QVector<QByteArray> writeDataList;

    int deviceAddress = 1;
    QTcpSocket *modbustcp = nullptr;
};

#endif // WIDGET_H
