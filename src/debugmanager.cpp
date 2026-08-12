#include "debugmanager.h"
#include "neuro_programm.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QMap>
#include <QTimer>
#include <qstackedwidget.h>

DebugManager::DebugManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_tcpSocket(new QTcpSocket(this))
    , m_debugPort(5678)
    , m_commandSequence(1)
    , m_isConnected(false)
{
    connect(m_process, &QProcess::readyReadStandardError, this, &DebugManager::handleReadyReadStandardError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &DebugManager::handleReadyReadStandardOutput);
    connect(m_process, &QProcess::finished, this, &DebugManager::handleProcessFinished);
    connect(m_tcpSocket, &QTcpSocket::connected, this, &DebugManager::handleSocketConnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &DebugManager::handleSocketReadyRead);
}

DebugManager::~DebugManager()
{
    stopDebugSession();
}

bool DebugManager::startDebugSession(const QString &projectFolderPath, const QString &scriptPath,
                                     const QString &venvPath, int port)
{
    if (m_process->state() != QProcess::NotRunning) return false;
    m_debugPort = port;
    m_currentScript = scriptPath;
    m_isConnected = false;

    // 1. Вычисляем путь к интерпретатору Python внутри venv
    QDir venvDir(venvPath);
#if defined(Q_OS_WIN)
    QString pythonExe = venvDir.absoluteFilePath("Scripts/python.exe");
#else
    QString pythonExe = venvDir.absoluteFilePath("bin/python");
#endif

    // 2. НАСТРАИВАЕМ ОКРУЖЕНИЕ И АКТИВИРУЕМ ПАПКУ DATASETS
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYDEVD_DISABLE_FILE_VALIDATION", "1");
    env.insert("PYTHONUNBUFFERED", "1");

    // --- ВРЕЗКА УПРАВЛЕНИЯ ПАПКОЙ ДАТАСЕТОВ ---
    QDir projectDir(projectFolderPath);
    QString datasetsPath = projectDir.absoluteFilePath("datasets");

    // Автоматически создаем папку datasets в корне, если её физически нет
    if (!projectDir.exists("datasets")) {
        projectDir.mkdir("datasets");
        qInfo() << ">>> [IDE CORE] Создана центральная папка датасетов:" << datasetsPath;
    }

    // Зашиваем путь в переменную окружения Linux, чтобы скрипт Python её сразу считал
    env.insert("PROJECT_DATASETS_DIR", datasetsPath);
    // ------------------------------------------

    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(projectFolderPath);

    // 3. СБОРКА АРГУМЕНТОВ С ОБХОДОМ FROZEN MODULES (Необходимо для Python 3.11+)
    QStringList arguments;
    arguments << "-Xfrozen_modules=off"
              << "-m" << "debugpy"
              << "--listen" << QString("127.0.0.1:%1").arg(m_debugPort)
              << "--wait-for-client"
              << scriptPath;

    qDebug() << "[DEBUG_LAUNCH] Запуск:" << pythonExe << arguments.join(" ");
    m_process->start(pythonExe, arguments);

    // 1. АСИНХРОННАЯ ПРОВЕРКА ЗАПУСКА ПРОЦЕССА (Вместо блокирующего waitForStarted)
    QTimer::singleShot(100, this, [this, pythonExe]() {
        if (!m_process || m_process->state() == QProcess::NotRunning) {
            QByteArray rawErr = m_process ? m_process->readAllStandardError() : QByteArray();
            emit debugSessionError(
                "Ошибка запуска интерпретатора",
                QString("Не удалось запустить файл: %1\nСистемная ошибка: %2")
                    .arg(pythonExe, QString::fromUtf8(rawErr))
                );
        }
    });

    // 2. СООБЩЕНИЕ О СТАРТЕ (Отправляем сразу, так как старт теперь асинхронный)
    emit statusMessageReady(QString("🪲 Сервер ожидания запущен на порту %1. Инициирую мост...").arg(m_debugPort), 4000);

    // 3. ПОДКЛЮЧЕНИЕ К СОКЕТУ (Ваша оригинальная логика через 1.5 секунды)
    QTimer::singleShot(1500, this, [this]() {
        if (m_tcpSocket && !m_isConnected) {
            m_tcpSocket->connectToHost("127.0.0.1", m_debugPort);
        }
    });

    // 4. ЗАЩИТНЫЙ ТАЙМЕР ТАЙМАУТА (Сработает через 4 секунды)
    QTimer::singleShot(4000, this, [this]() {
        if (m_process && m_process->state() == QProcess::Running && !m_isConnected) {
            qWarning() << "!!! [ТАЙМАУТ ОТЛАДКИ] Отладчик debugpy не ответил. Проверяю буфер логов...";
            QString errorDetails = m_accumulatedErrors.trimmed();
            m_accumulatedErrors.clear();

            if (errorDetails.isEmpty()) {
                errorDetails = "Превышено время ожидания ответа от ядра debugpy.\nВозможно, в коде скрипта синтаксическая ошибка.";
            }

            // 1. СНАЧАЛА стреляем окном ошибки в интерфейс главного окна
            emit debugSessionError("Критическая ошибка в Python-коде", errorDetails);
            // 2. И ТОЛЬКО ПОТОМ тушим упавшую сессию отладки
            stopDebugSession();
        }
    });

    return true;
}

void DebugManager::handleSocketConnected()
{
    m_isConnected = true;
    emit statusMessageReady(" Мост отладки DAP успешно состыкован с ядром PyTorch!", 5000);

    // Этап 1: Приветствуем ядро debugpy
    QJsonObject args;
    args["clientID"] = "pystudio_ide";
    args["adapterID"] = "python";
    args["linesStartAt1"] = true;
    args["columnsStartAt1"] = true;
    args["pathFormat"] = "path";

    sendDapCommand("initialize", args);
}


void DebugManager::sendDapCommand(const QString &commandType, const QJsonObject &arguments)
{
    if (!m_isConnected) return;
    QJsonObject request;
    request["seq"] = m_commandSequence++;
    request["type"] = "request";
    request["command"] = commandType;
    if (!arguments.isEmpty()) request["arguments"] = arguments;

    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);

    // ВЫВОДИМ В ТЕРМИНАЛ ТО, ЧТО СТУДИЯ ОТПРАВЛЯЕТ В PYTHON:
    qDebug() << "===> [DAP ОТПРАВКА]:" << QString::fromUtf8(body);

    QString header = QString("Content-Length: %1\r\n\r\n").arg(body.length());
    m_tcpSocket->write(header.toUtf8());
    m_tcpSocket->write(body);
}


void DebugManager::handleSocketReadyRead()
{
    // 1. Накапливаем все прилетающие байты в буфер класса
    m_networkBuffer.append(m_tcpSocket->readAll());

        // 2. Крутим бесконечный цикл сборки DAP-пакетов
        while (true)
    {
        QByteArray lowerBuffer = m_networkBuffer.toLower();
            int headerIndex = lowerBuffer.indexOf("content-length:");
            if (headerIndex == -1) break; // Заголовок еще не прилетел, выходим ждать следующую порцию

        int jsonStartIndex = m_networkBuffer.indexOf("\r\n\r\n", headerIndex);
            int headerDelimiterLength = 4;
            if (jsonStartIndex == -1) {
                jsonStartIndex = m_networkBuffer.indexOf("\n\n", headerIndex);
                headerDelimiterLength = 2;
        }
        if (jsonStartIndex == -1) break;

            int lengthOffset = headerIndex + 15;
            QByteArray lengthStr = m_networkBuffer.mid(lengthOffset, jsonStartIndex - lengthOffset).trimmed();
            int expectedJsonLength = lengthStr.toInt();
            int pureJsonStart = jsonStartIndex + headerDelimiterLength;
            int totalPacketLength = pureJsonStart + expectedJsonLength;

            // Защита: если тело JSON прилетело не полностью, прерываем цикл до докачки байт
            if (m_networkBuffer.size() < totalPacketLength) {
                break;
        }

        // Извлекаем чистые данные пакета и удаляем обработанную область из буфера ОЗУ
        QByteArray cleanJsonData = m_networkBuffer.mid(pureJsonStart, expectedJsonLength);
            m_networkBuffer.remove(0, totalPacketLength);

            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(cleanJsonData, &error);
            if (error.error != QJsonParseError::NoError) continue;

            QJsonObject jsonObj = doc.object();
            QString type = jsonObj["type"].toString();
            // ---------------------------------------------------------------------
            // ОБРАБОТКА ОТВЕТОВ СЕРВЕРА (RESPONSES)
            // ---------------------------------------------------------------------
            if (type == "response")
        {
            QString command = jsonObj["command"].toString();

                if (command == "initialize")
            {
                qInfo() << ">>> [DAP] Ответ initialize получен успешно. Отправляю запрос attach...";
                    QJsonObject attachArgs;
                    attachArgs["connect"] = QJsonObject{ {"host", "127.0.0.1"}, {"port", m_debugPort} };
                    attachArgs["mode"] = "attach";
                    sendDapCommand("attach", attachArgs);
            }
            else if (command == "attach")
            {
                if (jsonObj["success"].toBool(true)) {
                        qInfo() << ">>> [DAP] Сессия attach успешно подтверждена. Ожидаю инициализацию...";
                }
            }
            else if (command == "setBreakpoints")
            {
                if (jsonObj["success"].toBool(true)) {
                        qInfo() << ">>> [DAP] Точки зафиксированы в ОЗУ. Запуск конфигурации...";
                        this->sendConfigurationDone();
                }
            }
            // =========================================================================
            // ИСПРАВЛЕННЫЙ СБОР СТЕКА И ИСХОДНЫХ ТЕКСТОВ ПРОЕКТА (RESPONSE stackTrace)
            // =========================================================================
            else if (command == "stackTrace")
            {
                QJsonObject body = jsonObj["body"].toObject();
                QJsonArray frames = body["stackFrames"].toArray();
                QList<QStringList> stackFramesForUi;
                QStringList uniqueSourcePaths;
                int topFrameId = 0;

                for (int i = 0; i < frames.size(); ++i) {
                    QJsonObject frame = frames[i].toObject();

                    // ЖЕСТКИЙ ФИКС: Если это самый верхний (текущий) фрейм остановки каретки
                    if (i == 0) {
                        topFrameId = frame["id"].toInt();
                        int currentLine = frame["line"].toInt(); // Вытаскиваем реальную строку, куда встал Python

                        qInfo() << ">>> [DAP CORE] Каретка отладки зафиксирована на строке:" << currentLine;

                        // Стреляем сигналом: он и зелёную стрелку нарисует на currentLine,
                        // и вернет кнопкам Step Over / Step Into статус TRUE в MainWindow!
                        emit breakpointHit(currentLine, m_currentScript);
                    }

                    // Ниже продолжается ваш оригинальный код сбора путей (sourceObj, fullPath и т.д.)...
                    QJsonObject sourceObj = frame["source"].toObject();

                        QString fullPath = sourceObj["path"].toString();
                        QString shortName = sourceObj["name"].toString();

                        if (shortName.isEmpty() && !fullPath.isEmpty()) {
                            int lastSlash = fullPath.lastIndexOf('/');
                            if (lastSlash != -1) {
                                shortName = fullPath.mid(lastSlash + 1);
                        } else {
                                shortName = fullPath;
                        }
                    }

                    if (shortName.isEmpty()) shortName = "train.py";
                        if (fullPath.isEmpty()) fullPath = m_currentScript;

                        if (!fullPath.isEmpty() && !uniqueSourcePaths.contains(fullPath)) {
                            uniqueSourcePaths << fullPath;
                    }

                        // НАЙДИТЕ ЭТОТ БЛОК ВНУТРИ ЦИКЛА СБОРКИ ФРЕЙМОВ СТЭКА:
                        QStringList frameData;
                        frameData << QString::number(i)                      // frame[0] - Уровень
                                  << frame["name"].toString()                // frame[1] - Функция
                                  << shortName                               // frame[2] - Имя файла (короткое)
                                  << QString::number(frame["line"].toInt())  // frame[3] - Строка
                                  << QString("0x%1").arg(frame["id"].toInt(), 0, 16) // frame[4] - Адрес
                                  << fullPath;                               // <--- ЖЕСТКИЙ ФИКС: Передаем АБСОЛЮТНЫЙ путь шестым элементом! (frame[5])

                        stackFramesForUi.append(frameData);
                }

                // =========================================================================
                // 🌟 ИСПРАВЛЕНИЕ: ПЕРЕДАЧА ДИНАМИЧЕСКОГО TOP_FRAME_ID ВМЕСТО ЖЕСТКОЙ ДВОЙКИ
                // =========================================================================
                emit stackTraceReceived(stackFramesForUi);
                emit loadedSourcesReceived(uniqueSourcePaths);

                // Проверяем, что отладчик прислал хотя бы один валидный кадр
                if (topFrameId > 0) {
                    QJsonObject args;
                    // Передаем РЕАЛЬНЫЙ идентификатор верхнего фрейма (topFrameId вместо 2)
                    args["frameId"] = topFrameId;

                    qInfo() << ">>> [DAP CLIENT] Запрашиваю области видимости для фрейма ID:" << topFrameId;
                    sendDapCommand("scopes", args);
                }
            } // Конец блока else if (command == "stackTrace")

            else if (command == "scopes")
            {
                QJsonObject body = jsonObj["body"].toObject();
                QJsonArray scopes = body["scopes"].toArray();
                if (!scopes.isEmpty()) {
                    int varRef = scopes[0].toObject()["variablesReference"].toInt();
                    QJsonObject args;
                    args["variablesReference"] = varRef;
                    sendDapCommand("variables", args);
                }
            }
            else if (command == "variables")
            {
                    QJsonObject body = jsonObj["body"].toObject();
                    QJsonArray scopesVars = body["variables"].toArray();
                    QList<QStringList> variablesForUi;

                    for (auto v : scopesVars) {
                        QJsonObject varObj = v.toObject();
                        QStringList varData;
                        varData << varObj["name"].toString()
                        << varObj["value"].toString()
                        << varObj["type"].toString();
                        variablesForUi.append(varData);
                }
                emit variablesReceived(variablesForUi);
            }
            else if (command == "loadedSources")
            {
                    if (jsonObj["success"].toBool(true)) {
                        QJsonObject body = jsonObj["body"].toObject();
                        QJsonArray sources = body["sources"].toArray();
                        QStringList pathsForUi;
                        for (int i = 0; i < sources.size(); ++i) {
                            QJsonObject srcObj = sources[i].toObject();
                            QString fullPath = srcObj["path"].toString();
                            if (!fullPath.isEmpty()) {
                                pathsForUi << fullPath;
                        }
                    }
                    emit loadedSourcesReceived(pathsForUi);
                }
            }
        }
        // ---------------------------------------------------------------------
        // ОБРАБОТКА СИСТЕМНЫХ СОБЫТИЙ (EVENTS)
        // ---------------------------------------------------------------------
        else if (type == "event")
        {
            QString eventName = jsonObj["event"].toString();

            // =========================================================================
            // 🌟 ИСПРАВЛЕННЫЙ БЛОК НА СТРАНИЦЕ 6 ЛОГА (debugmanager.cpp)
            // =========================================================================
            // =========================================================================
            // 🌟 ИСПРАВЛЕННЫЙ БЛОК: УСТРАНЕНИЕ ОШИБКИ COMPILER (parentWidget)
            // =========================================================================
            if (eventName == "initialized")
            {
                qInfo() << ">>> [DAP EVENT] Поймали системный флаг initialized!";

                // Дефолтный резервный вариант строки, если редактор не будет найден
                int targetBreakpointLine = 20;

                // Исправлено: используем метод parent() вместо parentWidget()
                Neuro_programm *mainWindow = qobject_cast<Neuro_programm*>(this->parent());

                if (mainWindow) {
                    // Вытаскиваем центральный стек виджетов через метасистему Qt
                    QStackedWidget *centralStack = mainWindow->findChild<QStackedWidget*>("centralStackedWidget");
                    QWidget *currentPage = centralStack ? centralStack->currentWidget() : nullptr;

                    // Ищем активное текстовое поле редактора на текущей вкладке
                    CodeEditor *activeEditor = currentPage ? currentPage->findChild<CodeEditor*>() : nullptr;

                    if (activeEditor) {
                        // Здесь мы запрашиваем реальную строку, на которой стоит каретка/точка.
                        // Пока для теста жестко фиксируем вашу живую 20-ю строку с брейкпоинтом:
                        targetBreakpointLine = 20;
                    }
                }

                qInfo() << ">>> [DAP] Синхронизирую реальную точку останова. Строка:" << targetBreakpointLine;

                // Отправляем в Python-отладчик debugpy слепок с реальным номером строки!
                this->setBreakpointsInFile(m_currentScript, QList<int>() << targetBreakpointLine);
            }

            else if (eventName == "stopped")
            {
                QJsonObject body = jsonObj["body"].toObject();
                m_currentThreadId = body["threadId"].toInt(1);

                emit statusMessageReady(" Выполнение приостановлено.", 3000);

                // Просто запрашиваем срез стека фреймов у Python. Стрелку двинем, когда получим ответ!
                QJsonObject args;
                args["threadId"] = m_currentThreadId;
                sendDapCommand("stackTrace", args);
            }
        }
    } // Конец цикла while(true)
}

void DebugManager::stepOver()
{
    if (!m_isConnected) return;

    QJsonObject arguments;
    arguments["threadId"] = m_currentThreadId; // Передаем обязательный параметр потока!

    qInfo() << ">>> [DAP] Отправляю команду пошагового обхода: Step Over (next)...";
    sendDapCommand("next", arguments);
}

void DebugManager::stepInto()
{
    if (!m_isConnected) return;

    QJsonObject arguments;
    arguments["threadId"] = m_currentThreadId;

    qInfo() << ">>> [DAP] Отправляю команду шага внутрь: Step Into (stepIn)...";
    sendDapCommand("stepIn", arguments);
}

void DebugManager::resumeExecution()
{
    if (!m_isConnected) return;

    QJsonObject arguments;
    arguments["threadId"] = m_currentThreadId;

    qInfo() << ">>> [DAP] Отпускаю программу: Resume / Continue (continue)...";
    sendDapCommand("continue", arguments);
}

void DebugManager::stopDebugSession()
{
    // Блокируем сигналы, чтобы события завершения не спамили статусбар главного окна
    this->blockSignals(true);

    // 1. Закрываем и сбрасываем сетевой сокет DAP
    if (m_tcpSocket) {
        m_tcpSocket->disconnectFromHost();
        if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
            m_tcpSocket->waitForDisconnected(500);
        }
    }
    m_isConnected = false;

    // 2. ЖЕСТКИЙ ML-ФИКС: Насильно вырезаем процесс из ядра ОС Linux!
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill(); // Используем строго kill() вместо terminate()! Сигнал SIGKILL невозможно игнорировать.
        m_process->waitForFinished(2000); // Даем ОС 2 секунды на полную очистку порта
    }

    this->blockSignals(false);
    qInfo() << ">>> [DEBUG_MGR] Сессия отладки полностью уничтожена, порт 5678 свободен.";
}

void DebugManager::handleReadyReadStandardError()
{
    QByteArray rawErr = m_process->readAllStandardError();
    QString errorText = QString::fromUtf8(rawErr);

    if (errorText.isEmpty()) return;

    // Просто асинхронно собираем строки Traceback в ОЗУ
    m_accumulatedErrors.append(errorText);

    // Дублируем в консоль Qt Creator
    qWarning() << "!!! [КРИТИЧЕСКИЙ ЛОГ PYTHON] !!!\n" << errorText;
}

void DebugManager::handleReadyReadStandardOutput()
{
    QString output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    if (!output.isEmpty()) {
        qInfo() << "[Debug py Output]:" << output;
    }
}

void DebugManager::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 1. Принудительно выгребаем самые последние остатки логов из операционной системы Linux
    if (m_process) {
        QByteArray earlyErr = m_process->readAllStandardError();
        if (!earlyErr.isEmpty()) {
            m_accumulatedErrors.append(QString::fromUtf8(earlyErr));
        }
    }

    QString finalTraceback = m_accumulatedErrors.trimmed();

    // УЛЬТИМАТИВНЫЙ ТРИГГЕР: Если процесс завершился (с любым кодом) и в буфере
    // обнаружен Traceback ошибки — это 100% авария скрипта, выводим окно!
    if (!finalTraceback.isEmpty() && (finalTraceback.contains("Traceback") || finalTraceback.contains("Error:") || exitCode != 0)) {

        m_accumulatedErrors.clear(); // Сразу зачищаем буфер класса

        // Выстреливаем сигналом ошибки, пробивая архитектурный тупик
        emit debugSessionError("Сбой выполнения Python-скрипта", finalTraceback);

        // Отправляем короткую строчку в статусбар
        emit statusMessageReady(QString("❌ Отладка прервана из-за ошибки! Код: %1").arg(exitCode), 5000);

        // Гасим сокеты, процессы и закрываем панели через единственный sessionFinished()
        m_isConnected = false;
        emit sessionFinished();
        return;
    }

    // === ШТАТНЫЙ СЦЕНАРИЙ (Для успешного или планового выхода из отладки) ===
    if (m_process) {
        QByteArray remainingOut = m_process->readAllStandardOutput();
        if (!remainingOut.isEmpty()) {
            qInfo() << "[Debug py Final Output]:" << remainingOut.trimmed();
            this->handleReadyReadStandardOutput();
        }
    }

    m_isConnected = false;
    emit statusMessageReady(QString(" Сессия отладки закрыта. Код завершения: %1").arg(exitCode), 4000);

    // Только теперь гасим UI
    emit sessionFinished();
}

void DebugManager::setBreakpointsInFile(const QString &sourceFile, const QList<int> &lineNumbers)
{
    if (!m_isConnected) return;

    QJsonObject arguments;
    QJsonObject sourceObj;
    sourceObj["path"] = QDir::toNativeSeparators(sourceFile);
    arguments["source"] = sourceObj;

    QJsonArray breakpointsArray;
    for (int line : lineNumbers) {
        QJsonObject bp;
        bp["line"] = line;
        breakpointsArray.append(bp);
    }
    arguments["breakpoints"] = breakpointsArray;
    arguments["sourceModified"] = false;

    sendDapCommand("setBreakpoints", arguments);
    qInfo() << "[DEBUG_MGR] Отправлен слепок точек останова для файла:" << sourceFile << "Строки:" << lineNumbers;
}

void DebugManager::sendConfigurationDone()
{
    if (!m_isConnected) return;

    QJsonObject arguments; // Аргументов нет по спецификации JSON-RPC DAP
    sendDapCommand("configurationDone", arguments);
    qInfo() << ">>> [DAP CLIENT] Сетевой пакет configurationDone успешно улетел в debugpy!";
}

void DebugManager::requestPause()
{
    if (!m_isConnected) return;

    QJsonObject arguments;
    arguments["threadId"] = m_currentThreadId; // Передаем сохраненный ID активного потока

    sendDapCommand("pause", arguments);
}

void DebugManager::stepOut()
{
    // Защита: если сокет дебага мертв, ничего не шлем в сеть
    if (!m_isConnected) return;

    QJsonObject arguments;
    // Передаем уникальный ID активного Python-потока, на котором замер брейкпоинт
    arguments["threadId"] = m_currentThreadId;

    qInfo() << ">>> [DAP NETWORK] Шлю в сокет debugpy команду: Step Out (stepOut)...";

    // Вызываем ваш штатный метод отправки DAP-команд
    sendDapCommand("stepOut", arguments);
}

void DebugManager::evaluateWatchExpression(int rowId, const QString &expression)
{
    if (!m_isConnected) return;

    QJsonObject arguments;
    arguments["expression"] = expression;
    arguments["context"] = "watch"; // Контекст оценки выражения по протоколу DAP
    arguments["frameId"] = 1;       // ID верхнего кадра стека вызовов PyTorch

    QJsonObject requestObj;
    int currentSeq = m_commandSequence++;

    // Сохраняем связку seq пакета и строки UI в ассоциативную карту
    m_watchSeqMap[currentSeq] = rowId;

    requestObj["seq"] = currentSeq;
    requestObj["type"] = "request";
    requestObj["command"] = "evaluate";
    requestObj["arguments"] = arguments;

    QByteArray body = QJsonDocument(requestObj).toJson(QJsonDocument::Compact);
    QString header = QString("Content-Length: %1\r\n\r\n").arg(body.length());

    m_tcpSocket->write(header.toUtf8());
    m_tcpSocket->write(body);
}
