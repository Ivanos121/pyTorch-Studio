#include "usbreceiver.h"
#include <QDebug>

UsbReceiver::UsbReceiver(QObject *parent) : QObject(parent)
{
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &UsbReceiver::onReadyRead);
}

UsbReceiver::~UsbReceiver()
{
    disconnectNucleo();
}

bool UsbReceiver::connectToNucleo()
{
    if (m_serial->isOpen()) {
        m_serial->close();
    }

    QString targetPort = "";

    // Автоматический поиск платы Nucleo среди подключенных USB-устройств CDC
    const auto serialPortInfos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : serialPortInfos) {
        // Ищем по стандартным Linux-префиксам или системному ID STMicroelectronics
        if (portInfo.portName().contains("ttyACM") || portInfo.portName().contains("ttyUSB") ||
            portInfo.hasProductIdentifier() && portInfo.vendorIdentifier() == 0x0483) {
            targetPort = portInfo.portName();
            break;
        }
    }

    if (targetPort.isEmpty()) {
        emit connectionStatusChanged(false, "");
        return false;
    }

    m_serial->setPortName(targetPort);
    m_serial->setBaudRate(QSerialPort::Baud115200); // Скорость для USB Virtual COM Port
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadOnly)) {
        m_buffer.clear();
        emit connectionStatusChanged(true, targetPort);
        return true;
    }

    emit connectionStatusChanged(false, "");
    return false;
}

void UsbReceiver::disconnectNucleo()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit connectionStatusChanged(false, "");
    }
}

// АСИНХРОННЫЙ ПЕРЕХВАТ БАЙТ ИЗ miniUSB
void UsbReceiver::onReadyRead()
{
    // Читаем всё, что прилетело в буфер ОС из miniUSB
    m_buffer.append(m_serial->readAll());

    // Разбираем буфер посимвольно на строки (нейросеть обычно шлет логи с переносом строки '\n')
    while (m_buffer.contains('\n')) {
        int indexOfLineEnd = m_buffer.indexOf('\n');

        // Извлекаем чистую строку данных инференса
        QByteArray rawLine = m_buffer.left(indexOfLineEnd).trimmed();
        m_buffer.remove(0, indexOfLineEnd + 1);

        QString inferenceLine = QString::fromUtf8(rawLine);
        if (!inferenceLine.isEmpty()) {
            // Отправляем строку дальше в логику приложения для парсинга метрик или графиков
            emit inferenceDataReceived(inferenceLine);
        }
    }
}
