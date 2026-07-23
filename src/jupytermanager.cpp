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

void JupyterManager::startServer() {
    // 2. Извлекаем из pystudio.conf абсолютный путь к вашему внешнему venv Python
    QString pythonInterpreter = ConfigManager::instance().getValue("Python/InterpreterPath", "/usr/bin/python3").toString();

    // 3. Вычисляем путь к папке bin внутри venv
    int lastSlash = pythonInterpreter.lastIndexOf('/');
    QString venvBinDir = pythonInterpreter.left(lastSlash);

    // 4. Формируем изолированное Linux-окружение для виртуальной среды venv
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Внедряем venv/bin в начало системного PATH, чтобы Python видел локальные библиотеки
    QString currentPath = env.value("PATH");
    env.insert("PATH", venvBinDir + QDir::listSeparator() + currentPath);

    // Передаем маркер VIRTUAL_ENV (это завершает корректную активацию venv для подпроцессов)
    int binPos = venvBinDir.lastIndexOf("/bin");
    if (binPos != -1) {
        QString venvRootDir = venvBinDir.left(binPos);
        env.insert("VIRTUAL_ENV", venvRootDir);
    }

    m_process->setProcessEnvironment(env);

    // 5. Запускаем jupyter как модуль Python через абсолютный путь
    // Это на 100% страхует от системной ошибки "execve: Файл или каталог не существует"
    QStringList arguments;
    arguments << "-m" << "notebook" << "--no-browser";

    qDebug() << "[JUPYTER] Запуск сервера через:" << pythonInterpreter;

    m_process->start(pythonInterpreter, arguments);
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
