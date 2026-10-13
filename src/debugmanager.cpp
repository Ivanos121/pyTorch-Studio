#include "debugmanager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QMap>
#include <QTimer>

DebugManager::DebugManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_tcpSocket(new QTcpSocket(this))
    , m_debugPort(5678)
    , m_commandSequence(1)
    , m_isConnected(false)
    , m_currentThreadId(1)
{
    connect(m_process, &QProcess::readyReadStandardError, this, &DebugManager::handleReadyReadStandardError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &DebugManager::handleReadyReadStandardOutput);
    connect(m_process, &QProcess::finished, this, &DebugManager::handleProcessFinished);

    connect(m_tcpSocket, &QTcpSocket::connected, this, &DebugManager::handleSocketConnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &DebugManager::handleSocketReadyRead);
    connect(m_tcpSocket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        // Ошибки подключения при попытках коннекта игнорируем до успешной стыковки
    });
}

DebugManager::~DebugManager()
{
    this->disconnect();
    stopDebugSession();
}

bool DebugManager::startDebugSession(const QString &projectFolderPath, const QString &scriptPath,
                                     const QString &venvPath, int port)
{
    if (m_process->state() != QProcess::NotRunning) {
        return false;
    }

    m_debugPort = port;
    m_currentScript = scriptPath;
    m_isConnected = false;
    m_accumulatedErrors.clear();
    m_networkBuffer.clear();

    // 1. Вычисляем путь к интерпретатору Python внутри venv
    QDir venvDir(venvPath);
#if defined(Q_OS_WIN)
    QString pythonExe = venvDir.absoluteFilePath("Scripts/python.exe");
#else
    QString pythonExe = venvDir.absoluteFilePath("bin/python");
#endif

    // 2. Настраиваем окружение и активируем папку datasets
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYDEVD_DISABLE_FILE_VALIDATION", "1");
    env.insert("PYTHONUNBUFFERED", "1");
    env.insert("MPLBACKEND", "Agg");
    env.insert("QT_QPA_PLATFORM", "wayland");

    QDir projectDir(projectFolderPath);
    QString datasetsPath = projectDir.absoluteFilePath("datasets");
    if (!projectDir.exists("datasets")) {
        projectDir.mkdir("datasets");
    }
    qInfo() << ">>> [IDE CORE] Создана центральная папка датасетов:" << datasetsPath;
    env.insert("PROJECT_DATASETS_DIR", datasetsPath);

    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(projectFolderPath);

    // 3. Сборка аргументов с обходом frozen modules
    QString cleanScriptPath = QFileInfo(scriptPath).canonicalFilePath();
    if (cleanScriptPath.isEmpty()) {
        cleanScriptPath = QDir::cleanPath(scriptPath);
    }

    QStringList arguments;
    arguments << "-Xfrozen_modules=off"
              << "-m" << "debugpy"
              << "--listen" << QString("127.0.0.1:%1").arg(m_debugPort)
              << "--wait-for-client"
              << cleanScriptPath; // Передаем канонический путь

    m_process->start(pythonExe, arguments);

    if (!m_process->waitForStarted(2000)) {
        emit debugSessionError(
            "Ошибка запуска интерпретатора",
            QString("Не удалось запустить процесс Python: %1").arg(pythonExe)
            );
        return false;
    }

    emit statusMessageReady(QString("Сервер ожидания запущен на порту %1. Инициирую мост...").arg(m_debugPort), 4000);

    // 4. Безопасное циклическое подключение к порту вместо слепой задержки
    QTimer *retryTimer = new QTimer(this);
    QTimer *timeoutTimer = new QTimer(this);

    connect(retryTimer, &QTimer::timeout, this, [this, retryTimer]() {
        if (m_isConnected) {
            retryTimer->stop();
            retryTimer->deleteLater();
            return;
        }
        if (m_process && m_process->state() == QProcess::Running) {
            if (m_tcpSocket->state() == QAbstractSocket::UnconnectedState) {
                m_tcpSocket->connectToHost("127.0.0.1", m_debugPort);
            }
        }
    });

    // 5. Сторожевой таймер только на этап первого рукопожатия (15 сек для PyTorch)
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [this, retryTimer, timeoutTimer]() {
        retryTimer->stop();
        retryTimer->deleteLater();
        timeoutTimer->deleteLater();

        if (m_process && m_process->state() == QProcess::Running && !m_isConnected) {
            qWarning() << "!!! [ТАЙМАУТ ОТЛАДКИ] Отладчик debugpy не ответил за отведенное время.";
            QString errorDetails = m_accumulatedErrors.trimmed();
            m_accumulatedErrors.clear();
            if (errorDetails.isEmpty()) {
                errorDetails = "Превышено время ожидания ответа от ядра debugpy.\n"
                               "Возможно, скрипт долго импортирует библиотеки или содержит синтаксические ошибки.";
            }
            emit debugSessionError("Таймаут инициализации отладки", errorDetails);
            stopDebugSession();
        }
    });

    // При успешном коннекте глушим таймеры
    connect(m_tcpSocket, &QTcpSocket::connected, this, [retryTimer, timeoutTimer]() {
        if (retryTimer) {
            retryTimer->stop();
            retryTimer->deleteLater();
        }
        if (timeoutTimer) {
            timeoutTimer->stop();
            timeoutTimer->deleteLater();
        }
    });

    retryTimer->start(350);
    timeoutTimer->start(15000);

    return true;
}

void DebugManager::handleSocketConnected()
{
    m_isConnected = true;
    emit statusMessageReady("Мост отладки DAP успешно состыкован с ядром PyTorch!", 5000);

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
    if (!m_isConnected || !m_tcpSocket || m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject request;
    request["seq"] = m_commandSequence++;
    request["type"] = "request";
    request["command"] = commandType;
    if (!arguments.isEmpty()) {
        request["arguments"] = arguments;
    }

    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    qDebug() << "===> [DAP ОТПРАВКА]:" << QString::fromUtf8(body);

    QString header = QString("Content-Length: %1\r\n\r\n").arg(body.length());
    m_tcpSocket->write(header.toUtf8());
    m_tcpSocket->write(body);
    m_tcpSocket->flush();
}

void DebugManager::handleSocketReadyRead()
{
    m_networkBuffer.append(m_tcpSocket->readAll());

    while (true) {
        QByteArray lowerBuffer = m_networkBuffer.toLower();
        int headerIndex = lowerBuffer.indexOf("content-length:");
        if (headerIndex == -1) {
            break;
        }

        int jsonStartIndex = m_networkBuffer.indexOf("\r\n\r\n", headerIndex);
        int headerDelimiterLength = 4;
        if (jsonStartIndex == -1) {
            jsonStartIndex = m_networkBuffer.indexOf("\n\n", headerIndex);
            headerDelimiterLength = 2;
            if (jsonStartIndex == -1) {
                break;
            }
        }

        int lengthOffset = headerIndex + 15;
        QByteArray lengthStr = m_networkBuffer.mid(lengthOffset, jsonStartIndex - lengthOffset).trimmed();
        int expectedJsonLength = lengthStr.toInt();
        int pureJsonStart = jsonStartIndex + headerDelimiterLength;
        int totalPacketLength = pureJsonStart + expectedJsonLength;

        if (m_networkBuffer.size() < totalPacketLength) {
            break;
        }

        QByteArray cleanJsonData = m_networkBuffer.mid(pureJsonStart, expectedJsonLength);
        m_networkBuffer.remove(0, totalPacketLength);

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(cleanJsonData, &error);
        if (error.error != QJsonParseError::NoError) {
            continue;
        }

        QJsonObject jsonObj = doc.object();
        QString type = jsonObj["type"].toString();

        // =========================================================================
        // 1. ОБРАБОТКА ОТВЕТОВ (RESPONSES)
        // =========================================================================
        if (type == "response") {
            QString command = jsonObj["command"].toString();

            if (command == "initialize") {
                qInfo() << ">>> [DAP] Ответ initialize получен успешно. Отправляю запрос attach...";
                QJsonObject attachArgs;
                attachArgs["connect"] = QJsonObject{ {"host", "127.0.0.1"}, {"port", m_debugPort} };
                attachArgs["mode"] = "attach";
                sendDapCommand("attach", attachArgs);
            }
            else if (command == "attach") {
                if (jsonObj["success"].toBool(true)) {
                    qInfo() << ">>> [DAP] Сессия attach успешно подтверждена. Ожидаю initialized...";
                }
            }
            else if (command == "setBreakpoints") {
                qInfo().noquote() << ">>> [DAP ОТВЕТ НА ТОЧКИ]:"
                                  << QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);

                QJsonObject body = jsonObj["body"].toObject();
                QJsonArray breakpoints = body["breakpoints"].toArray();
                for (const QJsonValue &val : breakpoints) {
                    QJsonObject bp = val.toObject();
                    bool verified = bp["verified"].toBool();
                    int line = bp["line"].toInt();
                    qInfo() << ">>> [DAP BP STATUS] Строка:" << line
                            << "Подтверждена (verified):" << verified;
                    if (!verified && bp.contains("message")) {
                        qWarning() << ">>> [DAP BP ПРИЧИНА]:" << bp["message"].toString();
                    }
                }

                // Завершаем фазу конфигурирования после регистрации точек
                if (jsonObj["success"].toBool(true)) {
                    qInfo() << ">>> [DAP] Точки зафиксированы в debugpy. Отправляю configurationDone...";
                    this->sendConfigurationDone();
                }
            }
            else if (command == "configurationDone") {
                qInfo() << ">>> [DAP] Конфигурация подтверждена debugpy. Ожидаю остановки на точке...";
            }
            else if (command == "stackTrace") {
                QJsonObject body = jsonObj["body"].toObject();
                QJsonArray frames = body["stackFrames"].toArray();
                QList<QStringList> stackFramesForUi;
                QStringList uniqueSourcePaths;
                int topFrameId = 0;

                for (int i = 0; i < frames.size(); ++i) {
                    QJsonObject frame = frames[i].toObject();
                    if (i == 0) {
                        topFrameId = frame["id"].toInt();
                        int currentLine = frame["line"].toInt();
                        qInfo() << ">>> [DAP CORE] Каретка отладки зафиксирована на строке:" << currentLine;
                        emit breakpointHit(currentLine, m_currentScript);
                    }

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

                    QStringList frameData;
                    frameData << QString::number(i)
                              << frame["name"].toString()
                              << shortName
                              << QString::number(frame["line"].toInt())
                              << QString("0x%1").arg(frame["id"].toInt(), 0, 16)
                              << fullPath;
                    stackFramesForUi.append(frameData);
                }

                emit stackTraceReceived(stackFramesForUi);
                emit loadedSourcesReceived(uniqueSourcePaths);

                if (topFrameId > 0) {
                    QJsonObject args;
                    args["frameId"] = topFrameId;
                    qInfo() << ">>> [DAP CLIENT] Запрашиваю области видимости для фрейма ID:" << topFrameId;
                    sendDapCommand("scopes", args);
                }
            }
            else if (command == "scopes") {
                QJsonObject body = jsonObj["body"].toObject();
                QJsonArray scopes = body["scopes"].toArray();
                if (!scopes.isEmpty()) {
                    int varRef = scopes[0].toObject()["variablesReference"].toInt();
                    QJsonObject args;
                    args["variablesReference"] = varRef;
                    sendDapCommand("variables", args);
                }
            }
            else if (command == "variables") {
                QJsonObject body = jsonObj["body"].toObject();
                QJsonArray scopesVars = body["variables"].toArray();
                QList<QStringList> variablesForUi;

                for (const auto &v : scopesVars) {
                    QJsonObject varObj = v.toObject();
                    QStringList varData;
                    varData << varObj["name"].toString()
                            << varObj["value"].toString()
                            << varObj["type"].toString();
                    variablesForUi.append(varData);
                }
                emit variablesReceived(variablesForUi);
            }
            else if (command == "loadedSources") {
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
        // =========================================================================
        // 2. ОБРАБОТКА СОБЫТИЙ (EVENTS)
        // =========================================================================
        else if (type == "event") {
            QString eventName = jsonObj["event"].toString();

            if (eventName == "initialized") {
                qInfo() << ">>> [DAP EVENT] Поймали системный флаг initialized!";
                // Отправляем точки останова. Ответ на них (setBreakpoints) запустит sendConfigurationDone()
                if (!m_activeBreakpoints.isEmpty()) {
                    for (auto it = m_activeBreakpoints.begin(); it != m_activeBreakpoints.end(); ++it) {
                        this->setBreakpointsInFile(it.key(), it.value());
                    }
                } else {
                    this->setBreakpointsInFile(m_currentScript, QList<int>());
                }
            }
            else if (eventName == "stopped") {
                QJsonObject body = jsonObj["body"].toObject();
                m_currentThreadId = body["threadId"].toInt(1);
                emit statusMessageReady("Выполнение приостановлено.", 3000);

                QJsonObject args;
                args["threadId"] = m_currentThreadId;
                sendDapCommand("stackTrace", args);
            }
        }
    }
}

void DebugManager::stepOver()
{
    if (!m_isConnected) return;
    QJsonObject arguments;
    arguments["threadId"] = m_currentThreadId;
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

void DebugManager::stepOut()
{
    if (!m_isConnected) return;
    QJsonObject arguments;
    arguments["threadId"] = m_currentThreadId;
    qInfo() << ">>> [DAP NETWORK] Шлю в сокет debugpy команду: Step Out (stepOut)...";
    sendDapCommand("stepOut", arguments);
}

void DebugManager::resumeExecution()
{
    if (!m_isConnected) return;
    QJsonObject arguments;
    arguments["threadId"] = m_currentThreadId;
    qInfo() << ">>> [DAP] Отпускаю программу: Resume / Continue (continue)...";
    sendDapCommand("continue", arguments);
}

void DebugManager::requestPause()
{
    if (!m_isConnected) return;
    QJsonObject arguments;
    arguments["threadId"] = m_currentThreadId;
    sendDapCommand("pause", arguments);
}

void DebugManager::stopDebugSession()
{
    bool wasRunning = m_isConnected || (m_process && m_process->state() != QProcess::NotRunning);
    m_isConnected = false;

    if (m_tcpSocket) {
        m_tcpSocket->disconnect(this);
        if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
            m_tcpSocket->disconnectFromHost();
        }
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    qInfo() << ">>> [DEBUG_MGR] Сессия отладки полностью уничтожена, порт" << m_debugPort << "свободен.";

    // Отправляем сигнал только если сигналы не заблокированы и сессия реально была активна:
    if (wasRunning && !this->signalsBlocked()) {
        emit sessionFinished();
    }
}

void DebugManager::handleReadyReadStandardError()
{
    if (!m_process) return;
    QByteArray rawErr = m_process->readAllStandardError();
    QString errorText = QString::fromUtf8(rawErr);
    if (errorText.isEmpty()) return;

    m_accumulatedErrors.append(errorText);
    qWarning() << "!!! [КРИТИЧЕСКИЙ ЛОГ PYTHON] !!!\n" << errorText;
}

void DebugManager::handleReadyReadStandardOutput()
{
    if (!m_process) return;
    QByteArray out = m_process->readAllStandardOutput();
    if (!out.isEmpty()) {
        qInfo().noquote() << "[PYTHON STDOUT]:" << QString::fromUtf8(out).trimmed();
    }
}

void DebugManager::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);

    if (m_process) {
        QByteArray earlyErr = m_process->readAllStandardError();
        if (!earlyErr.isEmpty()) {
            m_accumulatedErrors.append(QString::fromUtf8(earlyErr));
        }
    }

    QString finalTraceback = m_accumulatedErrors.trimmed();
    m_isConnected = false;

    if (!finalTraceback.isEmpty() && (finalTraceback.contains("Traceback") ||
                                      finalTraceback.contains("Error:") || exitCode != 0)) {
        m_accumulatedErrors.clear();
        emit debugSessionError("Сбой выполнения Python-скрипта", finalTraceback);
        emit statusMessageReady(QString("Отладка прервана из-за ошибки! Код: %1").arg(exitCode), 5000);
        emit sessionFinished();
        return;
    }

    if (m_process) {
        QByteArray remainingOut = m_process->readAllStandardOutput();
        if (!remainingOut.isEmpty()) {
            qInfo() << "[Debugpy Final Output]:" << remainingOut.trimmed();
        }
    }

    emit statusMessageReady(QString("Сессия отладки закрыта. Код завершения: %1").arg(exitCode), 4000);
    emit sessionFinished();
}

void DebugManager::setBreakpointsInFile(const QString &sourceFile, const QList<int> &lineNumbers)
{
    if (!m_isConnected) return;

    QString cleanPath = QFileInfo(sourceFile).canonicalFilePath();
    if (cleanPath.isEmpty()) {
        cleanPath = QDir::cleanPath(sourceFile);
    }

    QJsonObject arguments;
    QJsonObject sourceObj;
    sourceObj["path"] = cleanPath;
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
    qInfo() << "[DEBUG_MGR] Зафиксирован брейкпоинт для пути:" << cleanPath << "Строки:" << lineNumbers;
}

void DebugManager::sendConfigurationDone()
{
    if (!m_isConnected) return;
    QJsonObject arguments;
    sendDapCommand("configurationDone", arguments);
    qInfo() << ">>> [DAP CLIENT] Сетевой пакет configurationDone успешно улетел в debugpy!";
}

void DebugManager::evaluateWatchExpression(int rowId, const QString &expression)
{
    if (!m_isConnected) return;

    QJsonObject arguments;
    arguments["expression"] = expression;
    arguments["context"] = "watch";
    arguments["frameId"] = 1;

    QJsonObject requestObj;
    int currentSeq = m_commandSequence++;
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

void DebugManager::updateBreakpoints(const QString &filePath, const QList<int> &lines) {
    m_activeBreakpoints[filePath] = lines;
    // Если уже подключены к сокету — сразу обновляем на лету в Python
    if (m_isConnected) {
        setBreakpointsInFile(filePath, lines);
    }
}