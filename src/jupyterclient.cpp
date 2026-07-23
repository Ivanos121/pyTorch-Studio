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

void JupyterClient::connectToJupyter(const QString &host, int port)
{
    m_host = host;
    m_port = port;
    m_isReady = false;

    // Формируем HTTP POST запрос к REST API Jupyter для инициализации новой сессии
    QUrl url(QString("http://%1:%2/api/sessions").arg(m_host).arg(m_port));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Описываем параметры сессии. Ядро просим запустить дефолтное (python3)
    QJsonObject json;
    json["kernel"] = QJsonObject{{"name", "python3"}};
    json["name"] = "pystudio_training_session";
    json["type"] = "notebook";

    m_networkManager->post(request, QJsonDocument(json).toJson());
    emit codeOutputReceived("🌐 [REST API] Отправлен запрос на инициализацию ядра Python...<br>");
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

    emit codeOutputReceived(QString("🔗 [REST API] Вычислительное ядро создано. ID: %1<br>").arg(m_kernelId));

    // Шаг 2: Подключаем бинарный WebSocket-канал к каналам управления этого ядра
    QString wsUrl = QString("ws://%1:%2/api/kernels/%3/channels").arg(m_host).arg(m_port).arg(m_kernelId);
    m_webSocket->open(QUrl(wsUrl));
}

void JupyterClient::onWebSocketConnected()
{
    m_isReady = true;
    emit codeOutputReceived("🔌 <font color='#00FF00'><b>[WebSockets] Соединение с ядром успешно установлено. Поток управления активен!</b></font><br>");
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
    // ПАРСИМ ПОТОК ОТВЕТОВ ОТ ЯДРА (IOPub channel)
    QJsonObject msgObj = QJsonDocument::fromJson(message.toUtf8()).object();
    QString msgType = msgObj["header"].toObject()["msg_type"].toString();
    QJsonObject content = msgObj["content"].toObject();

    // Случай 1: Ядро выводит стандартный поток (print() или логи PyTorch)
    if (msgType == "stream") {
        QString text = content["text"].toString();
        emit codeOutputReceived(text.toHtmlEscaped());
    }
    // Случай 2: Ядро вывело результат вычисления выражения
    else if (msgType == "execute_result") {
        QString res = content["data"].toObject()["text/plain"].toString();
        emit codeOutputReceived("<font color='#00FF00'>" + res.toHtmlEscaped() + "</font>\n");
    }
    // Случай 3: Критическая ошибка выполнения Python-кода (Трейсбэк ошибки обучения)
    else if (msgType == "error") {
        QJsonArray traceback = content["traceback"].toArray();
        QString errorStr;
        for (int i = 0; i < traceback.size(); ++i) {
            errorStr += traceback[i].toString() + "\n";
        }
        emit codeOutputReceived("<br><font color='#FF5350'><b>[PYTHON CRASH]:</b><br>" + errorStr.toHtmlEscaped() + "</font><br>");
        emit executionFinished(false);
    }
    // Случай 4: Статус ответа на нашу команду execute_reply
    else if (msgType == "execute_reply") {
        QString status = content["status"].toString();
        emit executionFinished(status == "ok");
    }
}

void JupyterClient::onWebSocketError(QAbstractSocket::SocketError error)
{
    emit codeOutputReceived(QString("<font color='red'>❌ Ошибка WebSocket канала: %1 (%2)</font><br>")
                                .arg(error).arg(m_webSocket->errorString()));
}
