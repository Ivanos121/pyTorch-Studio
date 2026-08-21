#include "jupyterclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDebug>

JupyterClient::JupyterClient(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
    m_webSocket(new QWebSocket()), m_port(8888), m_isReady(false)
{
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &JupyterClient::onSessionCreated);
    connect(m_webSocket, &QWebSocket::connected, this, &JupyterClient::onWebSocketConnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &JupyterClient::onWebSocketMessageReceived);

    // Безопасное приведение сигналов перегрузки ошибок в Qt5/Qt6
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &JupyterClient::onWebSocketError);
}

JupyterClient::~JupyterClient()
{
    if (m_webSocket->isValid()) {
        m_webSocket->close();
    }
    delete m_webSocket;
}

void JupyterClient::connectToJupyter(const QString &host, int port, const QString &notebookPath)
{
    m_host = host;
    m_port = port;
    m_isReady = false;

    // [ИСПРАВЛЕНИЕ 1]: Тотальное уничтожение старого сокета и его системных буферов!
    if (m_webSocket) {
        m_webSocket->abort();
        QObject::disconnect(m_webSocket, nullptr, nullptr, nullptr);
        m_webSocket->deleteLater();
    }

    // Создаем абсолютно чистый сокет для новой сессии обучения
    m_webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_webSocket, &QWebSocket::connected, this, &JupyterClient::onWebSocketConnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &JupyterClient::onWebSocketMessageReceived);
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, &JupyterClient::onWebSocketError);

    QUrl url(QStringLiteral("http://%1:%2/api/sessions").arg(m_host, QString::number(m_port)));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QString originUrl = QStringLiteral("http://%1:%2").arg(m_host, QString::number(m_port));
    request.setRawHeader("Origin", originUrl.toUtf8());

    QJsonObject json;
    json[QStringLiteral("kernel")] = QJsonObject{{QStringLiteral("name"), QStringLiteral("python3")}};
    json[QStringLiteral("name")] = QStringLiteral("pystudio_training_session");
    json[QStringLiteral("type")] = QStringLiteral("notebook");
    json[QStringLiteral("path")] = notebookPath;

    m_networkManager->post(request, QJsonDocument(json).toJson(QJsonDocument::Compact));
    emit codeOutputReceived(QStringLiteral(">>> [MLOps СЕТЬ]: Отправлен запрос на инициализацию ядра Python...\n"));
}

void JupyterClient::onSessionCreated(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit codeOutputReceived(QString("<font color='red'>❌ Ошибка REST API Jupyter: %1</font><br>").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    // Парсим ответ и вытаскиваем уникальный kernel_id созданного процесса
    QByteArray responseData = reply->readAll();
    QJsonObject jsonObj = QJsonDocument::fromJson(responseData).object();
    m_kernelId = jsonObj["kernel"].toObject()["id"].toString();
    reply->deleteLater();

    if (m_kernelId.isEmpty()) {
        emit codeOutputReceived("<font color='red'>❌ Ошибка: Сервер не вернул валидный Kernel ID.</font><br>");
        return;
    }

    emit codeOutputReceived(QString(" [REST API] Вычислительное ядро создано. ID: %1<br>").arg(m_kernelId));

    // Шаг 2: Подключаем бинарный WebSocket-канал к каналам управления этого ядра
    QString wsUrl = QString("ws://%1:%2/api/kernels/%3/channels").arg(m_host).arg(m_port).arg(m_kernelId);

    // =========================================================================
    // СОВРЕМЕННЫЙ ФИКС: Оформляем сокет-подключение как NetworkRequest с заголовком Origin
    // =========================================================================
    QNetworkRequest wsRequest((QUrl(wsUrl)));
    QString originUrl = QStringLiteral("http://%1:%2").arg(m_host, QString::number(m_port));
    wsRequest.setRawHeader("Origin", originUrl.toUtf8());

    // Открываем WebSocket с передачей настроенного запроса безопасности
    m_webSocket->open(wsRequest);
    // =========================================================================
}

void JupyterClient::onWebSocketConnected()
{
    m_isReady = true;
    emit codeOutputReceived(">>> [WebSockets] Соединение с ядром успешно установлено. Поток активен!\n");
    //  НОВАЯ СТРОКА: Сообщаем главному окну, что сокет на 100% готов отправлять код!
    emit jupyterClientReady();
}

void JupyterClient::executePythonCode(const QString &code)
{
    if (!m_isReady || m_kernelId.isEmpty()) {
        emit codeOutputReceived("<font color='red'>❌ Ошибка: Нет стабильного подключения к ядру Jupyter!</font><br>");
        return;
    }

    // ГЕНЕРИРУЕМ СТРУКТУРУ КАНpatch ПАКЕТА ПО СТАНДАРТУ JUPYTER MESSAGING SPECIFICATION v5
    QJsonObject message;

    // А. Заголовок сообщения
    QJsonObject header;
    header["msg_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    header["username"] = "pystudio_ide";
    header["session"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    header["msg_type"] = "execute_request"; // Наш главный тип запроса на выполнение
    header["version"] = "5.3";
    message["header"] = header;
    message["parent_header"] = QJsonObject();
    message["metadata"] = QJsonObject();

    // Б. Контент (Содержит сам чистый исполняемый код Python)
    QJsonObject content;
    content["code"] = code;
    content["silent"] = false;
    content["store_history"] = true;
    content["user_expressions"] = QJsonObject();
    content["allow_stdin"] = false;
    message["content"] = content;

    message["channel"] = "shell";

    // Отправляем JSON-пакет по WebSocket каналу в ядро
    m_webSocket->sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
}

void JupyterClient::onWebSocketMessageReceived(const QString &message)
{
    QJsonObject msgObj = QJsonDocument::fromJson(message.toUtf8()).object();
    QString msgType = msgObj[QStringLiteral("header")].toObject()[QStringLiteral("msg_type")].toString();
    QJsonObject content = msgObj[QStringLiteral("content")].toObject();

    static const QRegularExpression ansiRegex(QStringLiteral("\x1B\\[[0-9;]*[a-zA-Z]"));

    // Случай 1: Стандартный поток вывода (Вывод эпох, логов и этапов MLOps)
    if (msgType == QStringLiteral("stream")) {
        QString text = content[QStringLiteral("text")].toString();
        text.remove(ansiRegex);
        emit codeOutputReceived(text);

        // [ИСПРАВЛЕНИЕ 2]: Ловим финальный маркер успешного завершения прямо из текстового потока Python.
        // Как только скрипт вывел финальную строку — заявляем об успешном финише конвейера!
        if (text.contains(QStringLiteral("MLOPS FINAL SUCCESS"))) {
            emit executionFinished(true);
        }
    }
    // Случай 2: Результат вычисления выражений
    else if (msgType == QStringLiteral("execute_result")) {
        QString res = content[QStringLiteral("data")].toObject()[QStringLiteral("text/plain")].toString();
        res.remove(ansiRegex);
        emit codeOutputReceived(QStringLiteral("<font color='#00FF00'>") + res.toHtmlEscaped() + QStringLiteral("</font>\n"));
    }
    // Случай 3: Критическая ошибка (Сбой обучения, синтаксис, OOM на видеокарте)
    else if (msgType == QStringLiteral("error")) {
        QJsonArray traceback = content[QStringLiteral("traceback")].toArray();
        QString errorStr;
        for (int i = 0; i < traceback.size(); ++i) {
            QString line = traceback[i].toString();
            line.remove(ansiRegex);
            errorStr += line + QStringLiteral("\n");
        }
        emit codeOutputReceived(QStringLiteral("<br><font color='#FF5350'><b>[PYTHON CRASH]:</b><br>") + errorStr.toHtmlEscaped() + QStringLiteral("</font><br>"));

        // Сигнализируем о падении конвейера
        emit executionFinished(false);
    }
    // Случай 4: Технический статус ответа ядра на команду execute_reply
    else if (msgType == QStringLiteral("execute_reply")) {
        QString status = content[QStringLiteral("status")].toString();
        if (status != QStringLiteral("ok")) {
            // Если статус не OK (например, abort), генерируем ошибку
            emit executionFinished(false);
        }
        // [ИСПРАВЛЕНИЕ 3]: Если статус "ok", мы его ИГНОРИРУЕМ.
        // Больше он не будет дублировать успешный финиш и ломать рамочки в интерфейсе!
    }
}

void JupyterClient::onWebSocketError(QAbstractSocket::SocketError error)
{
    emit codeOutputReceived(QString("<font color='red'>❌ Ошибка WebSocket канала: %1 (%2)</font><br>")
                                .arg(error).arg(m_webSocket->errorString()));
}
