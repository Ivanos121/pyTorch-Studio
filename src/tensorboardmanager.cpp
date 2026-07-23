#include "tensorboardmanager.h"
#include <QDir>
#include <QDebug>

TensorBoardManager::TensorBoardManager(QObject *parent)
    : QObject(parent), m_process(new QProcess(this)), m_currentPort(6006)
{
    connect(m_process, &QProcess::readyReadStandardError, this, &TensorBoardManager::handleReadyReadStandardError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TensorBoardManager::handleReadyReadStandardOutput);
    connect(m_process, &QProcess::errorOccurred, this, &TensorBoardManager::handleProcessError);
    connect(m_process, &QProcess::finished, this, &TensorBoardManager::handleProcessFinished);
}

TensorBoardManager::~TensorBoardManager()
{
    stopServer();
}

bool TensorBoardManager::startServer(const QString &projectFolderPath, int port)
{
    if (m_process->state() != QProcess::NotRunning) {
        return true; // Сервер уже запущен
    }

    m_projectPath = projectFolderPath;
    m_currentPort = port;
    QDir dir(m_projectPath);

    QStringList arguments;
    // Настраиваем жесткую привязку к папке logs/ нашего проекта
    arguments << QString("--logdir=%1").arg(dir.absoluteFilePath("logs"))
              << QString("--port=%1").arg(m_currentPort)
              << "--host=127.0.0.1"
              << "--reload_interval=5"; // Интервал обновления графиков в секундах

    m_process->setWorkingDirectory(m_projectPath);

    // Запуск процесса в зависимости от операционной системы
#if defined(Q_OS_WIN)
    m_process->start("tensorboard.exe", arguments);
#else
    m_process->start("tensorboard", arguments); // Стандарт для Arch Linux
#endif

    if (!m_process->waitForStarted(5000)) {
        emit boardErrorOccurred("Не удалось запустить исполняемый файл TensorBoard.");
        return false;
    }

    emit boardLogReceived(QString("📈 <b>[TENSORBOARD] Мониторинг логов запущен на порту %1</b><br>").arg(m_currentPort));
    return true;
}

void TensorBoardManager::stopServer()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        emit boardLogReceived("<br>🛑 <b>[TENSORBOARD] Остановка сервера мониторинга...</b><br>");
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
            m_process->waitForFinished();
        }
    }
}

bool TensorBoardManager::isRunning() const
{
    return (m_process && m_process->state() == QProcess::Running);
}

void TensorBoardManager::handleReadyReadStandardError()
{
    QString output = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    if (!output.isEmpty()) {
        emit boardLogReceived("[TensorBoard Core]: " + output + "\n");
    }
}

void TensorBoardManager::handleReadyReadStandardOutput()
{
    QString output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    if (!output.isEmpty()) {
        emit boardLogReceived("[TensorBoard Out]: " + output + "\n");
    }
}

void TensorBoardManager::handleProcessError(QProcess::ProcessError error)
{
    QString msg = QString("Сбой процесса TensorBoard: %1 (%2)").arg(error).arg(m_process->errorString());
    emit boardErrorOccurred(msg);
}

void TensorBoardManager::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    emit boardLogReceived(QString("<br>🏁 <b>[TENSORBOARD] Процесс завершен. Код: %1, Статус: %2</b><br>")
                              .arg(exitCode).arg(exitStatus == QProcess::NormalExit ? "Штатно" : "Сбой"));
}
