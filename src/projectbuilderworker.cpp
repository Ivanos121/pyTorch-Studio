#include "projectbuilderworker.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QProcess>

ProjectBuilderWorker::ProjectBuilderWorker(const QString &projectPath,
                                           const QString &projectName,
                                           bool useGpu,
                                           bool useCustomReq,
                                           const QString &customReqPath,
                                           const QString &customVenvPath,
                                           bool isExistingVenvMode,
                                           QObject *parent)
    : QObject(parent),
    m_projectPath(projectPath),
    m_projectName(projectName),
    m_useGpu(useGpu),
    m_useCustomReq(useCustomReq),
    m_customReqPath(customReqPath),
    m_customVenvPath(customVenvPath),
    m_isExistingVenvMode(isExistingVenvMode)
{
}

void ProjectBuilderWorker::startBuildPipeline()
{
    emit logOutputReceived("🚀 <b>[СТАРТ] Начало сборки MLOps-окружения проекта " + m_projectName + "</b><br>");

    // СТАДИЯ 1: GIT И .GITIGNORE (Общая для всех режимов)
    if (!initializeGitRepository()) return;

    QString finalVenvPath;

    // ВЕТВЛЕНИЕ В ЗАВИСИМОСТИ ОТ ВЫБОРА ПОЛЬЗОВАТЕЛЯ
    if (m_isExistingVenvMode) {
        // РЕЖИМ А: Использование внешнего существующего venv
        emit logOutputReceived("🔗 <b>[СТАДИЯ 2] Проверка существующего виртуального окружения...</b>");

        if (!validateExistingEnvironment(m_customVenvPath)) {
            emit pipelineBuildFinished(false, "Указанная папка не является валидным venv (отсутствует интерпретатор).");
            return;
        }

        finalVenvPath = m_customVenvPath; // Фиксируем оригинальный внешний путь
        emit logOutputReceived("  • Окружение успешно валидировано по адресу: <font color='#00FF00'>" + finalVenvPath + "</font>");
        emit progressStepChanged(50, "Окружение привязано");

    } else {
        // РЕЖИМ Б: Создание нового venv внутри каталога проекта (Ваш старый код)
        emit logOutputReceived("📦 <b>[СТАДИЯ 2] Создание нового изолированного venv в каталоге проекта...</b>");

        if (!createVirtualEnvironment(finalVenvPath)) return;
    }

    // СТАДИЯ 3: УСТАНОВКА ПАКЕТОВ (PIP)
    // Если venv существующий — мы накатываем кастомные requirements (если пользователь попросил).
    // Если venv новый — ставим базовый стек PyTorch.
    if (!installMLOpsDependencies(finalVenvPath)) return;

    // СТАДИЯ 4: РЕГИСТРАЦИЯ В JUPYTER (Фоновое ядро связывается с выбранным finalVenvPath)
    if (!registerJupyterKernel(finalVenvPath)) return;

    // ФИНАЛ УСПЕХА
    emit logOutputReceived("<br>✅ <b>[УСПЕХ] Весь конвейер MLOps успешно развернут под ключ!</b><br>");
    emit progressStepChanged(100, "Готово");
    emit pipelineBuildFinished(true, "Проект успешно инициализирован.");
}


bool ProjectBuilderWorker::initializeGitRepository()
{
    emit progressStepChanged(1, "Инициализация Git");
    emit logOutputReceived("🔧 [1/4] Выполняется 'git init' в корне проекта...<br>");

    if (!runSystemCommand("git", QStringList() << "init", m_projectPath)) {
        emit pipelineBuildFinished(false, "Ошибка при инициализации Git.");
        return false;
    }

    emit logOutputReceived("📝 Генерируется интеллектуальный файл .gitignore...<br>");
    QFile gitignore(m_projectPath + "/.gitignore");
    if (gitignore.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&gitignore);
        out << "venv/\n.venv/\n__pycache__/\n*.pyc\n"
            << "datasets/\nweights/\nlogs/\n.ipynb_checkpoints/\n";
        gitignore.close();
    }
    emit logOutputReceived("✅ [1/4] Репозиторий Git успешно зарегистрирован.<br><br>");
    return true;
}

bool ProjectBuilderWorker::createVirtualEnvironment(QString &outVenvPath)
{
    emit progressStepChanged(2, "Создание виртуального окружения");

    QString rawPath = m_customVenvPath.trimmed();

    // Убрана блокировка пути "/home/elf/venv"
    if (rawPath.isEmpty() || !QDir::isAbsolutePath(rawPath) ||
        rawPath == "venv" || rawPath.endsWith("окружение"))
    {
        outVenvPath = m_projectPath + "/venv"; // Фиксируем внутри проекта только при ошибках
    } else {
        outVenvPath = rawPath; // Теперь здесь спокойно запишется /home/elf/venv
    }

    // --- ВОТ ЭТОТ БЛОК ОБЯЗАТЕЛЬНО ДОЛЖЕН БЫТЬ ТУТ: ---
    QStringList venvArgs;
    venvArgs << "-m" << "venv" << outVenvPath;

    // Запуск команды создания окружения в ОС
    if (!runSystemCommand("python3", venvArgs, m_projectPath)) {
        emit pipelineBuildFinished(false, "Сбой при создании виртуального окружения через python3 -m venv.");
        return false;
    }

    return true;
}

bool ProjectBuilderWorker::installMLOpsDependencies(const QString &venvPath)
{
    emit progressStepChanged(3, "Установка PyTorch & Библиотек");
    emit logOutputReceived(" [3/4] Обновление базового пакетного менеджера pip... \n");

    // КРОССПЛАТФОРМЕННЫЙ ПУТЬ К PIP (Защита от падения на Windows)
#ifdef Q_OS_WIN
    QString pipPath = venvPath + "/Scripts/pip.exe";
#else
    QString pipPath = venvPath + "/bin/pip";
#endif

    // Шаг А: Обновление pip, setuptools и wheel
    QStringList upgradeArgs;
    upgradeArgs << "install" << "--upgrade" << "pip" << "setuptools" << "wheel";
    if (!runSystemCommand(pipPath, upgradeArgs, m_projectPath)) {
        emit pipelineBuildFinished(false, "Сбой при обновлении пакетного менеджера pip.");
        return false;
    }

    // Шаг Б: Установка PyTorch и MLOps библиотек
    emit logOutputReceived(" Скачивание и сборка MLOps-зависимостей... \n");

    QStringList pipArgs;
    pipArgs << "install" << "--default-timeout=100";

    // Базовый набор библиотек
    pipArgs << "torch" << "torchvision" << "transformers" << "tensorboard" << "jupyter" << "ipykernel";

    // Используем EXTRA-index-url, чтобы не отключать стандартный PyPI репозиторий
    if (m_useGpu) {
        emit logOutputReceived(" Выбрана конфигурация GPU/CUDA. Начинается загрузка PyTorch конвейера... \n");
        pipArgs << "--extra-index-url" << "https://pytorch.org";
    } else {
        emit logOutputReceived(" Выбрана легкая конфигурация CPU. Начинается загрузка whl-зеркала... \n");
        pipArgs << "--extra-index-url" << "https://download.pytorch.org/whl/cpu";
    }

    if (!runSystemCommand(pipPath, pipArgs, m_projectPath)) {
        emit pipelineBuildFinished(false, "Сбой при установке pip-пакетов. Проверьте интернет-соединение или кэш pip.");
        return false;
    }

    emit logOutputReceived(" [3/4] Все MLOps-зависимости (PyTorch, TensorBoard, HF) успешно установлены.<br><br>");
    return true;
}

bool ProjectBuilderWorker::registerJupyterKernel(const QString &venvPath)
{
    emit progressStepChanged(4, "Регистрация ядра Jupyter");
    emit logOutputReceived("🔧 [4/4] Интеграция окружения venv в Jupyter Notebook...<br>");

    QString pythonPath = venvPath + "/bin/python";
    QStringList jupyterArgs;
    jupyterArgs << "-m" << "ipykernel" << "install" << "--user"
                << "--name" << m_projectName.toLower() + "_env"
                << "--display-name" << QString("Python (%1-venv)").arg(m_projectName);

    if (!runSystemCommand(pythonPath, jupyterArgs, m_projectPath)) {
        emit pipelineBuildFinished(false, "Не удалось зарегистрировать ipykernel в системе.");
        return false;
    }
    return true;
}

bool ProjectBuilderWorker::runSystemCommand(const QString &program, const QStringList &arguments, const QString &workingDir)
{
    QProcess process;
    process.setWorkingDirectory(workingDir);
    process.start(program, arguments);

    connect(&process, &QProcess::readyReadStandardOutput, this, [this, &process]() {
        QString out = QString::fromUtf8(process.readAllStandardOutput());
        emit logOutputReceived(out);
    });

    connect(&process, &QProcess::readyReadStandardError, this, [this, &process]() {
        QString err = QString::fromUtf8(process.readAllStandardError());
        emit logOutputReceived("<span style='color:#ef5350;'>" + err + "</span>");
    });

    process.waitForFinished(-1);
    return (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0);
}

bool ProjectBuilderWorker::validateExistingEnvironment(const QString &venvPath)
{
    if (venvPath.isEmpty()) return false;

    QDir venvDir(venvPath);

    // В зависимости от ОС исполняемый файл Python лежит в разных подпапках
#if defined(Q_OS_WIN)
    QString pythonBinaryPath = venvDir.absoluteFilePath("Scripts/python.exe");
#else
    QString pythonBinaryPath = venvDir.absoluteFilePath("bin/python");
#endif

    // Проверяем физическое существование файла на диске
    if (!QFile::exists(pythonBinaryPath)) {
        emit logOutputReceived("<font color='#FF0000'>❌ Ошибка: Интерпретатор Python не найден по пути: " + pythonBinaryPath + "</font>");
        return false;
    }

    return true;
}
