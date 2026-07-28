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
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QFontInfo>

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
        if (logFile.isOpen()) {
            logFile.flush(); // Просто сбрасываем буферы на диск
            logFile.close();
        }
        abort();
    }
}


int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORMTHEME", "kde");
    qputenv("XDG_CURRENT_DESKTOP", "KDE");
    qputenv("FONTCONFIG_FILE", "/etc/fonts/fonts.conf");
    qputenv("FONTCONFIG_PATH", "/etc/fonts");

    // Разрешаем Chromium загружать JS-модули xterm.js напрямую с жесткого диска
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-web-security --allow-file-access-from-files");

    // Отключаем размытие шрифтов при масштабировании интерфейса
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);
#endif

    QCoreApplication::setOrganizationName(QStringLiteral("elf"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("io.github.elf"));
    QCoreApplication::setApplicationName(QStringLiteral("pytorch-studio"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("pytorch-studio"));
    QGuiApplication::setDesktopFileName(QStringLiteral("pytorch-studio"));

    QApplication a(argc, argv);

    const QString systemIconPath = QDir::home().absoluteFilePath(QStringLiteral(".local/share/icons/hicolor/scalable/apps/pytorch-studio.svg"));
    const QString fallbackResourcePath = QStringLiteral(":/Data/Icons/pytorch-studio.svg");

    QIcon appIcon = QIcon::fromTheme(QStringLiteral("pytorch-studio"));

    // Резервный вариант: если программа запущена в режиме разработки и ярлыки еще не установлены
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral(":/Data/Icons/pytorch-studio.svg"));
    }

    a.setWindowIcon(appIcon);

    QFont globalFixedFont;
    globalFixedFont.setFamily("Liberation Mono"); // Ваш проверенный шрифт
    globalFixedFont.setStyleHint(QFont::TypeWriter);
    globalFixedFont.setFixedPitch(true);
    globalFixedFont.setPointSize(10);

    // Накатываем шрифт глобально на все будущие виджеты программы
    a.setFont(globalFixedFont);

    QFontInfo appFontInfo(a.font());
    qDebug() << "================== ТЕСТ ЯДРА QT (main.cpp) ==================";
    qDebug() << "Реальное семейство шрифта приложения:" << appFontInfo.family();
    qDebug() << "Флаг моноширинности (Fixed Pitch):"
             << (appFontInfo.fixedPitch() ? " ДА, МОНОШИРИННЫЙ" : " НЕТ, ПРОПОРЦИОНАЛЬНЫЙ");
    qDebug() << "============================================================";

    // Формируем строковую версию из макросов сборщика: "2026.1-LTS"
    QString appVersion = QString("%1.%2-%3")
                             .arg(APP_VERSION_MAJOR)
                             .arg(APP_VERSION_MINOR)
                             .arg(APP_VERSION_PATCH);
    QApplication::setApplicationVersion(appVersion);

    qRegisterMetaType<QList<QuickFixAction>>("QList<QuickFixAction>");

    // =========================================================================
    // АВТОМАТИЧЕСКИЙ ДИНАМИЧЕСКИЙ ПОИСК КОРНЯ ПРОЕКТА (БЕЗ МАКРОСОВ И СТРОК)
    // =========================================================================
    QFileInfo mainFileInfo(__FILE__);
    QDir currentDir = mainFileInfo.absoluteDir();
    QString projectPath = currentDir.absolutePath();

    // На случай, если main.cpp лежит в подпапке (например, src/), поднимемся до корня репозитория
    for (int i = 0; i < 5; ++i)
    {
        if (currentDir.exists("pyTorch-Studio.pro") ||
            currentDir.exists("CMakeLists.txt") ||
            currentDir.dirName() == "pyTorch-Studio")
        {
            projectPath = currentDir.absolutePath(); // Нашли реальный корень репозитория!
            break;
        }
        if (!currentDir.cdUp()) {
            break;
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

    // ОТКРЫТИЕ ФАЙЛА В РЕЖИМЕ ТЕКСТА НА ЗАПИСЬ
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "КРИТИЧЕСКАЯ ОШИБКА: Не удалось открыть файл для записи логов!" << std::endl;
    }

    // Устанавливаем наш обработчик сообщений
    qInstallMessageHandler(linuxConsoleMessageHandler);

    qInfo() << "PyTorch Studio запуск... Сетевые и графические интерфейсы инициализированы.";

    // =========================================================================
    // ИЗМЕНЕНИЕ: Сбор и перехват внешних аргументов запуска ОС
    // =========================================================================
    QString startupPath = "";
    QStringList args = QApplication::arguments();

    // Если размер аргументов больше 1, значит ОС передала путь к открываемому файлу
    if (args.size() > 1) {
        startupPath = args.at(1);
        qInfo() << "[SYSTEM_START] Обнаружен внешний аргумент пути запуска:" << startupPath;
    }

    // Передаем перехваченный путь в конструктор главного окна IDE
    Neuro_programm w(startupPath);
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
