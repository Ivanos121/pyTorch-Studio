#ifndef JUPYTERMANAGER_H
#define JUPYTERMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

class JupyterManager : public QObject
{
    Q_OBJECT
public:
    explicit JupyterManager(QObject *parent = nullptr);
    ~JupyterManager();

    // Запуск фонового Jupyter Notebook Server
    void startServer();

    // Мягкая остановка фонового сервера
    void stopServer();

    // Проверка текущего состояния процесса
    bool isRunning() const;
    int getCurrentPort() const { return m_currentPort; }

signals:
    // Сигнал передачи логов ядра в наш зеленый logEdit терминал
    void jupyterLogReceived(const QString &text);

    // Сигнал критического сбоя процесса
    void serverErrorOccurred(const QString &errorMsg);

private slots:
    void handleReadyReadStandardError();
    void handleReadyReadStandardOutput();
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_process;
    int m_currentPort;
    QString m_projectPath;
};

#endif // JUPYTERMANAGER_H
