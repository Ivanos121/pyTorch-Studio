#include "neuro_programm.h"

#include <QApplication>
#include <QDateTime>
#include <QStyleFactory>
#include <iostream>

// main.cpp
void linuxConsoleMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    std::string colorCode = "";
    std::string typeStr = "";

    switch (type) {
    case QtDebugMsg:    colorCode = "\033[36m"; typeStr = "[DEBUG]"; break;
    case QtInfoMsg:     colorCode = "\033[32m"; typeStr = "[INFO] "; break;
    case QtWarningMsg:  colorCode = "\033[33m"; typeStr = "[WARN] "; break;
    case QtCriticalMsg:
    case QtFatalMsg:
        colorCode = "\033[31m"; // Красный цвет для внешней консоли Arch Linux
        typeStr = "[ERROR]";
        break;
    }

    std::string resetCode = "\033[0m";

    // Выводим лог во внешний системный терминал
    std::cout << colorCode << timestamp.toStdString() << " " << typeStr << " "
              << msg.toStdString() << " (" << context.file << ":" << context.line << ")"
              << resetCode << std::endl;

    // =========================================================================
    // МАГИЯ АВТО-ОТКРЫТИЯ ВСТРОЕННОЙ КОНСОЛИ НА ЭКРАНЕ ПРИ СБОЕ
    // =========================================================================
    if (type == QtCriticalMsg || type == QtFatalMsg)
    {
        // Проверяем, что главное окно уже создано и запущено на экране
        if (Neuro_programm::self != nullptr) {
            // Передаем строку ошибки напрямую в графическое поле consoleOutput
            Neuro_programm::self->forceOpenConsoleWithError(msg);
        }
    }

    if (type == QtFatalMsg) abort();
}


int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORMTHEME", "kde");
    qputenv("XDG_CURRENT_DESKTOP", "KDE");

    QApplication a(argc, argv);

    qInstallMessageHandler(linuxConsoleMessageHandler);

    qInfo() << "PyTorch Studio запуск... Сетевые и графические интерфейсы инициализированы.";

    Neuro_programm w;
    //w.setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

    w.showMaximized();
    return QApplication::exec();
}
