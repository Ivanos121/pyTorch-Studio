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

void JupyterManager::startThermalTraining(const QString &projectRootPath)
{
    // 1. ЗАЩИТА: проверяем, создан ли процесс в памяти
    if (!m_process) {
        m_process = new QProcess(this);
    }

    // Если процесс уже запущен (например, идет старое обучение) — останавливаем его
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }

    // 2. ИЗВЛЕКАЕМ НАСТРОЙКИ ИЗ ВАШЕГО LINUX-КОНФИГА (~/.config/pystudio.conf)
    // Извлекаем абсолютный путь к вашему внешнему venv Python, который вы выставили в GUI
    QString pythonInterpreter = ConfigManager::instance().getValue("Python/InterpreterPath", "/usr/bin/python3").toString();

    // Вычисляем путь к папке bin внутри venv, чтобы прокинуть переменные окружения Linux
    int lastSlash = pythonInterpreter.lastIndexOf('/');
    QString venvBinDir = pythonInterpreter.left(lastSlash);

    // 3. ФОРМИРУЕМ ИЗОЛИРОВАННОЕ ОКРУЖЕНИЕ ДЛЯ VENV
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Внедряем venv/bin в начало системного PATH, чтобы Python видел локальные библиотеки (torch, torchvision)
    QString currentPath = env.value("PATH");
    env.insert(QStringLiteral("PATH"), venvBinDir + QDir::listSeparator() + currentPath);

    // Передаем маркер VIRTUAL_ENV для корректной работы библиотек внутри venv
    int binPos = venvBinDir.lastIndexOf("/bin");
    if (binPos != -1) {
        QString venvRootDir = venvBinDir.left(binPos);
        env.insert(QStringLiteral("VIRTUAL_ENV"), venvRootDir);
    }
    m_process->setProcessEnvironment(env);

    // 4. СТРОИМ НОВЫЙ СПИСОК АРГУМЕНТОВ ДЛЯ СКРИПТА
    QStringList arguments;

    // Вместо аргументов сервера Jupyter передаем относительный путь к нашему Python-скрипту
    // Файл scripts/thermal_train.py лежит внутри структуры вашего проекта z1
    arguments << QStringLiteral("scripts/thermal_train.py");

    // Жестко фиксируем рабочую директорию в корне проекта z1,
    // чтобы пути "./data/raw" внутри Python-скрипта отсчитывались корректно
    m_process->setWorkingDirectory(projectRootPath);

    qDebug() << "[ЯДРО C++] Запуск обучения теплограмм...";
    qDebug() << "[ЯДРО C++] Интерпретатор:" << pythonInterpreter;
    qDebug() << "[ЯДРО C++] Рабочая папка проекта:" << projectRootPath;

    // 5. ЗАПУСКАЕМ ПРОЦЕСС
    // Запускаем интерпретатор Python, передав ему путь к скрипту в аргументах
    m_process->start(pythonInterpreter, arguments);
}
