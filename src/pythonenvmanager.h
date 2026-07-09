#pragma once
#include <QObject>
#include <QThread>
#include <QString>
#include <QStringList>

// Класс-воркер, который будет физически жить и выполняться в фоновом потоке
class VenvWorker : public QObject {
    Q_OBJECT
public:
    explicit VenvWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void doCascadeSearch(const QString &projectPath);

signals:
    void searchFinished(bool success, const QString &pythonPath, const QStringList &packages, const QString &errorMsg);
};

// Класс-менеджер, управляющий жизненным циклом потока (вызывается из GUI)
class PythonEnvManager : public QObject {
    Q_OBJECT
private:
    QThread *m_workerThread;
    VenvWorker *m_worker;
    QString m_currentPythonPath;
    QStringList m_cachedPackages;

public:
    explicit PythonEnvManager(QObject *parent = nullptr);
    ~PythonEnvManager();

    // Главная точка входа для запуска фонового поиска
    void startBackgroundCheck(const QString &projectPath);

    QString currentPythonPath() const { return m_currentPythonPath; }
    QStringList cachedPackages() const { return m_cachedPackages; }

signals:
    void venvConnectedSuccessfully(const QStringList &packages);
    void venvNotFoundOrCorrupted(const QString &reason);
};
