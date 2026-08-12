#include "tensorboardmanager.h"
#include <QDir>
#include <QDebug>
#include <QTextStream> // Необходим для построчного чтения

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
        return true;
    }

    m_projectPath = projectFolderPath;
    m_currentPort = port;
    QDir dir(m_projectPath);

    QStringList arguments;
    arguments << QString("--logdir=%1").arg(dir.absoluteFilePath("logs"))
              << QString("--port=%1").arg(m_currentPort)
              << "--host=127.0.0.1"
              << "--reload_interval=5";

    m_process->setWorkingDirectory(m_projectPath);

#if defined(Q_OS_WIN)
    m_process->start("tensorboard.exe", arguments);
#else
    m_process->start("tensorboard", arguments);
#endif

    // Блокирующий вызов на 5 секунд в GUI-потоке — это нормально только при старте,
    // но если сервер не запустится, интерфейс замрет на 5 секунд.
    if (!m_process->waitForStarted(3000)) { // Снизили до 3 секунд для отзывчивости
        emit boardErrorOccurred("Не удалось запустить исполняемый файл TensorBoard.");
        return false;
    }

    emit boardLogReceived(QString("<b>[TENSORBOARD] Мониторинг логов запущен на порту %1</b><br>").arg(m_currentPort));
    return true;
}

void TensorBoardManager::stopServer()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        emit boardLogReceived("<br><b>[TENSORBOARD] Остановка сервера мониторинга...</b><br>");

        m_process->terminate();
        // Чтобы GUI не зависал при закрытии, используем небольшие таймауты
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
            m_process->waitForFinished(500);
        }
    }
}

bool TensorBoardManager::isRunning() const
{
    return (m_process && m_process->state() == QProcess::Running);
}

// ИСПРАВЛЕННЫЙ ВАРИАНТ: Построчное чтение потока ошибок
void TensorBoardManager::handleReadyReadStandardError()
{
    // Привязываем поток к стандартному выводу ошибок процесса
    QTextStream stream(m_process->readAllStandardError());
    stream.setEncoding(QStringConverter::Utf8); // Гарантируем UTF-8 для Qt6

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            emit boardLogReceived("[TensorBoard Core]: " + line + "\n");
        }
    }
}

// ИСПРАВЛЕННЫЙ ВАРИАНТ: Построчное чтение стандартного потока
void TensorBoardManager::handleReadyReadStandardOutput()
{
    QTextStream stream(m_process->readAllStandardOutput());
    stream.setEncoding(QStringConverter::Utf8);

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            emit boardLogReceived("[TensorBoard Out]: " + line + "\n");
        }
    }
}

void TensorBoardManager::handleProcessError(QProcess::ProcessError error)
{
    QString msg = QString("Сбой процесса TensorBoard: %1 (%2)").arg(error).arg(m_process->errorString());
    emit boardErrorOccurred(msg);
}

void TensorBoardManager::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString statusStr = (exitStatus == QProcess::NormalExit) ? "Штатно" : "Сбой";
    emit boardLogReceived(QString("<br><b>[TENSORBOARD] Процесс завершен. Код: %1, Статус: %2</b><br>")
                              .arg(exitCode).arg(statusStr));
}
