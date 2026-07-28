#include "jupytermanager.h"
#include "configmanager.h"

#include <QDir>
#include <QDebug>

JupyterManager::JupyterManager(QObject *parent)
    : QObject(parent), m_process(new QProcess(this)), m_currentPort(8888)
{
    // Подключаем чтение вывода ядра и перехват ошибок
    connect(m_process, &QProcess::readyReadStandardError, this, &JupyterManager::handleReadyReadStandardError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &JupyterManager::handleReadyReadStandardOutput);
    connect(m_process, &QProcess::errorOccurred, this, &JupyterManager::handleProcessError);
    connect(m_process, &QProcess::finished, this, &JupyterManager::handleProcessFinished);
}

JupyterManager::~JupyterManager()
{
    stopServer();
}

void JupyterManager::startServer(const QString &projectRootPath)
{
    if (m_process->state() == QProcess::Running) return;

    // 1. Формируем путь к бинарнику jupyter
    QString jupyterBin = QDir::home().absoluteFilePath(QStringLiteral("venv/bin/jupyter"));

    if (!QFile::exists(jupyterBin)) {
        jupyterBin = projectRootPath + QStringLiteral("/venv/bin/jupyter");
    }

    if (!QFile::exists(jupyterBin)) {
        jupyterBin = QStringLiteral("jupyter");
    }

    // =========================================================================
    // ОБЯЗАТЕЛЬНО СЮДА: Объявляем переменную arguments, которой не хватало!
    // =========================================================================
    QStringList arguments;

    arguments << QStringLiteral("server") // Теперь смело используем современный server!
              << QStringLiteral("--no-browser")
              << QStringLiteral("--port=8888")
              << QStringLiteral("--ip=127.0.0.1")
              << QStringLiteral("--IdentityProvider.token=")
              << QStringLiteral("--ServerApp.allow_origin=*")
              << QStringLiteral("--ServerApp.disable_check_xsrf=True")
              << (QStringLiteral("--ServerApp.root_dir=") + projectRootPath);

    // Наполняем список аргументами запуска сервера
    m_process->setWorkingDirectory(projectRootPath);

    // 3. Запускаем процесс
    m_process->start(jupyterBin, arguments);
}



void JupyterManager::stopServer()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        emit jupyterLogReceived("<br>🛑 <b>[JUPYTER] Запрос вежливой остановки фонового сервера...</b><br>");
        m_process->terminate(); // Посылаем сигнал SIGTERM

        if (!m_process->waitForFinished(4000)) {
            emit jupyterLogReceived("⚠️ [JUPYTER] Сервер не ответил на SIGTERM. Принудительное уничтожение процесса (SIGKILL)...<br>");
            m_process->kill();  // Посылаем жесткий SIGKILL
            m_process->waitForFinished();
        }
    }
}

bool JupyterManager::isRunning() const
{
    return (m_process && m_process->state() == QProcess::Running);
}

void JupyterManager::handleReadyReadStandardError()
{
    // Jupyter Notebook Server выводит всю сервисную отладку (включая шаги PyTorch) в канал ошибок
    QString output = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    if (!output.isEmpty()) {
        emit jupyterLogReceived("[Jupyter Core]: " + output + "\n");
    }
}

void JupyterManager::handleReadyReadStandardOutput()
{
    QString output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    if (!output.isEmpty()) {
        emit jupyterLogReceived("[Jupyter Out]: " + output + "\n");
    }
}

void JupyterManager::handleProcessError(QProcess::ProcessError error)
{
    QString msg = QString("Критический сбой QProcess ядра Jupyter. Код ошибки OS: %1 (%2)")
                      .arg(error).arg(m_process->errorString());
    emit serverErrorOccurred(msg);
}

void JupyterManager::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    emit jupyterLogReceived(QString("<br>🏁 <b>[JUPYTER] Фоновый сервер завершил работу. Код: %1, Статус: %2</b><br>")
                                .arg(exitCode).arg(exitStatus == QProcess::NormalExit ? "Штатно" : "Сбой"));
}
