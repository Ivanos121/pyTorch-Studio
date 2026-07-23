#ifndef JUPYTERCLIENT_H
#define JUPYTERCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebSocket>
#include <QUuid>

class JupyterClient : public QObject
{
    Q_OBJECT
public:
    explicit JupyterClient(QObject *parent = nullptr);
    ~JupyterClient();

    // Шаг 1: Подключение к серверу и запрос создания сессии ядра
    void connectToJupyter(const QString &host, int port);

    // Шаг 2: Отправка Python-кода на выполнение в фоновое ядро
    void executePythonCode(const QString &code);

signals:
    // Сигнал возврата текстового вывода (print(), логи PyTorch) в зеленый logEdit терминал
    void codeOutputReceived(const QString &text);
    // Сигнал об успешном окончании выполнения ячейки
    void executionFinished(bool success);

private slots:
    void onSessionCreated(QNetworkReply *reply);
    void onWebSocketConnected();
    void onWebSocketMessageReceived(const QString &message);
    void onWebSocketError(QAbstractSocket::SocketError error);

private:
    QNetworkAccessManager *m_networkManager;
    QWebSocket *m_webSocket;

    QString m_host;
    int m_port;
    QString m_kernelId; // ID живого ядра Python
    bool m_isReady;     // Готовность сокета к отправке команд
};

#endif // JUPYTERCLIENT_H
