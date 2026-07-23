#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

class UsbReceiver : public QObject
{
    Q_OBJECT
public:
    explicit UsbReceiver(QObject *parent = nullptr);
    ~UsbReceiver();

    bool connectToNucleo(); // Метод для открытия порта miniUSB
    void disconnectNucleo();

signals:
    // Сигнал передает готовую разобранную строчку данных от нейросети в главное окно
    void inferenceDataReceived(const QString &line);
    void connectionStatusChanged(bool connected, const QString &portName);

private slots:
    void onReadyRead(); // Слот асинхронного чтения байт из USB

private:
    QSerialPort *m_serial;
    QByteArray m_buffer; // Буфер для накопления незавершенных пакетов данных
};
