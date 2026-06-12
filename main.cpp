#include "neuro_programm.h"
#include <QApplication>
#include <QDateTime>
#include <QStyleFactory>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <iostream>
#include <QMutex>       // ОБЯЗАТЕЛЬНО: Подключаем мьютекс для защиты потоков!
#include <QMutexLocker>

// =========================================================================
// ШАГ 1.1: СОЗДАЕМ ГЛОБАЛЬНЫЙ ОБЪЕКТ ФАЙЛА ДЛЯ ДОСТУПА ИЗ ХЕНДЛЕРА
// =========================================================================
static QFile logFile;
static QMutex logMutex;

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

    // -------------------------------------------------------------------------
    // ЗАЩИЩЕННАЯ ПОТОКОБЕЗОПАСНАЯ ЗАПИСЬ В ТЕКСТОВЫЙ ЛОГ (ФИКС ДЛЯ JEDI)
    // -------------------------------------------------------------------------
    {
        // КРИТИЧЕСКИЙ ШАГ: Блокируем файл для текущего потока.
        // Если фоновый поток Jedi сейчас пишет лог, GUI-поток подождет долю микросекунды.
        QMutexLocker locker(&logMutex);

        if (logFile.isOpen()) {
            QTextStream stream(&logFile);
            stream << timestamp << " " << QString::fromStdString(typeStr) << " " << msg;

            // Если компилятор передал имя файла исходника, дописываем его в лог
            if (context.file) {
                stream << " (" << context.file << ":" << context.line << ")";
            }
            stream << "\n";
            stream.flush(); // Выталкиваем байты на диск мгновенно
        }
    } // Здесь locker автоматически открывает замок мьютекса для других потоков

    // Вывод лога во внешний системный терминал (Ваш родной код)
    std::cout << colorCode << timestamp.toStdString() << " " << typeStr << " "
              << msg.toStdString() << " (" << (context.file ? context.file : "") << ":" << context.line << ")"
              << resetCode << std::endl;

    // Магия авто-открытия встроенной консоли на экране при сбое
    if (type == QtCriticalMsg || type == QtFatalMsg) {
        if (Neuro_programm::self != nullptr) {
            Neuro_programm::self->forceOpenConsoleWithError(msg);
        }
    }
    if (type == QtFatalMsg) {
        QMutexLocker locker(&logMutex);
        if (logFile.isOpen()) logFile.close();
        abort();
    }
}


int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORMTHEME", "kde");
    qputenv("XDG_CURRENT_DESKTOP", "KDE");
    QApplication a(argc, argv);

    // =========================================================================
    // АВТОМАТИЧЕСКИЙ ДИНАМИЧЕСКИЙ ПОИСК КОРНЯ ПРОЕКТА (БЕЗ МАКРОСОВ И СТРОК)
    // =========================================================================
    // 1. Берем папку, в которой лежит запущенный бинарник программы
    QDir currentDir(QCoreApplication::applicationDirPath());
    QString projectPath = currentDir.absolutePath(); // Запасной вариант по умолчанию

    // 2. Поднимаемся вверх по каталогам Linux (максимум на 5 уровней, чтобы не зависнуть)
    for (int i = 0; i < 5; ++i)
    {
        // Если имя текущей папки совпадает с именем корня вашего проекта
        if (currentDir.dirName() == "pyTorch-Studio") {
            projectPath = currentDir.absolutePath(); // Нашли! Фиксируем абсолютный путь
            break;
        }
        // Если не нашли — поднимаемся на один уровень выше (к родителю)
        if (!currentDir.cdUp()) {
            break; // Если дошли до корня диска "/", останавливаем поиск
        }
    }

    QString logDirPath = projectPath + "/Logs";
    QDir logDir(logDirPath);

    // 2. Создаем папку, если она не существует
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // 3. Формируем имя файла внутри этой папки
    QString logFileName = QString("%1/application_log_%2.txt")
                              .arg(logDirPath)
                              .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));

    // =========================================================================
    // ШАГ 1.3: ПРИВЯЗЫВАЕМ ИМЯ И ПРИНУДИТЕЛЬНО ОТКРЫВАЕМ ПОТОК НА ЗАПИСЬ
    // =========================================================================
    logFile.setFileName(logFileName);

    if (logFile.exists()) {
        (void)logFile.remove();
    }

    // ОТКРЫВАЕМ ФАЙЛ В РЕЖИМЕ ТЕКСТА НА ЗАПИСЬ (БЕЗ ЭТОГО ЛОГИ НЕ СОЗДАДУТСЯ!)
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "КРИТИЧЕСКАЯ ОШИБКА: Не удалось открыть файл для записи логов!" << std::endl;
    }

    // Устанавливаем наш обработчик сообщений
    qInstallMessageHandler(linuxConsoleMessageHandler);

    qInfo() << "PyTorch Studio запуск... Сетевые и графические интерфейсы инициализированы.";

    Neuro_programm w;
    w.showMaximized();

    int execResult = QApplication::exec();

    // =========================================================================
    // ШАГ 1.4: ВЕЖЛИВО ЗАКРЫВАЕМ ФАЙЛ ПРИ ВЫХОДЕ ИЗ ПРИЛОЖЕНИЯ
    // =========================================================================
    if (logFile.isOpen()) {
        logFile.close();
    }

    return execResult;
}

// int main(int argc, char *argv[])
// {
//     qputenv("QT_QPA_PLATFORMTHEME", "kde");
//     qputenv("XDG_CURRENT_DESKTOP", "KDE");

//     QApplication a(argc, argv);

//     // 1. Определяем путь к папке Logs относительно исполняемого файла
//     QString projectPath = QString(PROJECT_PATH);
//     QString logDirPath = projectPath + "/Logs";

//     QDir logDir(logDirPath);

//     // 2. Создаем папку, если она не существует
//     if (!logDir.exists())
//     {
//         logDir.mkpath(".");
//     }

//     // 3. Формируем имя файла внутри этой папки
//     QString logFileName = QString("%1/application_log_%2.txt")
//                               .arg(logDirPath)
//                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));

//     logFile.setFileName(logFileName);

//     // Сбрасываем файл (удаляем старый, если существует, и создаем новый)
//     if (logFile.exists()) {
//         (void)logFile.remove();
//     }

//     qInstallMessageHandler(linuxConsoleMessageHandler);

//     qInfo() << "PyTorch Studio запуск... Сетевые и графические интерфейсы инициализированы.";

//     Neuro_programm w;
//     //w.setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

//     w.showMaximized();
//     return QApplication::exec();
// }
