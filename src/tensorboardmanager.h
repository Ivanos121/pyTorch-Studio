#ifndef TENSORBOARDMANAGER_H
#define TENSORBOARDMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

class TensorBoardManager : public QObject
{
    Q_OBJECT
public:
    explicit TensorBoardManager(QObject *parent = nullptr);
    ~TensorBoardManager();

    // Запуск фонового процесса TensorBoard
    bool startServer(const QString &projectFolderPath, int port = 6006);

    // Остановка сервера
    void stopServer();

    // Текущий статус
    bool isRunning() const;
    int getCurrentPort() const { return m_currentPort; }

signals:
    // Сигнал для вывода логов инициализации в зеленый терминал logEdit
    void boardLogReceived(const QString &text);

    // Сигнал критической ошибки (например, порт занят)
    void boardErrorOccurred(const QString &errorMsg);

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

#endif // TENSORBOARDMANAGER_H
