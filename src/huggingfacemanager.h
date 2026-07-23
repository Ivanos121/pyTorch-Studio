#ifndef HUGGINGFACEMANAGER_H
#define HUGGINGFACEMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

class HuggingFaceManager : public QObject
{
    Q_OBJECT
public:
    explicit HuggingFaceManager(QObject *parent = nullptr);
    ~HuggingFaceManager();

    // Загрузка модели или датасета с хаба в локальный кэш проекта
    bool downloadAsset(const QString &projectFolderPath, const QString &repoId, const QString &filename = "");

    // Авторизация на хабе по токену пользователя (для приватных весов/датасетов)
    bool loginWithToken(const QString &projectFolderPath, const QString &token);

    // Проверка текущей активности процесса скачивания
    bool isBusy() const;
    void cancelOperation();

signals:
    // Сигнал передачи логов скачивания/авторизации в наш зеленый logEdit терминал
    void hfLogReceived(const QString &text);

    // Сигнал об успешном окончании или сбое операции
    void operationFinished(bool success, const QString &message);

private slots:
    void handleReadyReadStandardError();
    void handleReadyReadStandardOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessError(QProcess::ProcessError error);

private:
    // Вспомогательный метод настройки изолированного окружения HF_HOME
    QProcessEnvironment createIsolatedEnvironment(const QString &projectFolderPath);

    QProcess *m_process;
    QString m_projectPath;
    bool m_isBusy;
};

#endif // HUGGINGFACEMANAGER_H
