#ifndef DEBUGMANAGER_H
#define DEBUGMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTcpSocket>
#include <QJsonObject>
#include <QList>
#include <QStringList>

class DebugManager : public QObject
{
    Q_OBJECT

public:
    explicit DebugManager(QObject *parent = nullptr);
    ~DebugManager();

    // Запуск асинхронной сессии дебаггера Microsoft debugpy
    bool startDebugSession(const QString &projectFolderPath, const QString &scriptPath,
                           const QString &venvPath, int port = 5678);

    // Принудительный останов сессии отладки
    void stopDebugSession();

    // Команды управления шагами дебаггера
    void stepOver();
    void stepInto();
    void resumeExecution();
    void stepOut();

    // Передача слепка активных точек останова в ядро Python
    void setBreakpointsInFile(const QString &sourceFile, const QList<int> &lineNumbers);
    bool isConnected() const { return m_isConnected; }
    void sendConfigurationDone();
    void requestPause();
    void evaluateWatchExpression(int rowId, const QString &expression);

signals:
    void statusMessageReady(const QString &message, int timeout = 0);
    void breakpointHit(int line, const QString &sourceFile);
    void stackTraceReceived(const QList<QStringList> &stackFrames);
    void variablesReceived(const QList<QStringList> &variables);
    void sessionFinished();
    void loadedSourcesReceived(const QStringList &sourcePaths);
    void watchResultReady(int rowId, const QString &resultValue);
    void debugSessionError(QString title, QString details);
    void syntaxValidationFinished(QString filePath, QStringList errors);


private slots:
    // Обработчики потоков ввода-вывода QProcess
    void handleReadyReadStandardError();
    void handleReadyReadStandardOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // Обработчики сетевого сокета DAP
    void handleSocketConnected();
    void handleSocketReadyRead();

private:
    // Отправка низкоуровневой команды по спецификации DAP
    void sendDapCommand(const QString &commandType, const QJsonObject &arguments = QJsonObject());

    QProcess *m_process;
    QTcpSocket *m_tcpSocket;
    int m_debugPort;
    int m_commandSequence;
    bool m_isConnected;
    QString m_currentScript;
    QByteArray m_networkBuffer;
    int m_currentThreadId = 1;
    QMap<int, int> m_watchSeqMap;
    QString m_accumulatedErrors;
};

#endif // DEBUGMANAGER_H
