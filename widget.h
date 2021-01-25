#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QSerialPort>

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
    QByteArray mcuData;  //查询03数据包

    QVector<quint16> mcuRegisterValue;
    QVector<QByteArray> writeDataList;
};

#endif // WIDGET_H
