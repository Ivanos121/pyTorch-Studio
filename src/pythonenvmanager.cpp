#include "pythonenvmanager.h"
#include <QProcess>
#include <QFile>
#include <QSettings>
#include <QDebug>
#include <QDir>

PythonEnvManager::PythonEnvManager(QObject *parent)
    : QObject(parent), m_workerThread(nullptr), m_worker(nullptr) {}

PythonEnvManager::~PythonEnvManager() {
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void PythonEnvManager::startBackgroundCheck(const QString &projectPath)
{
    if (m_workerThread)
    {
        m_workerThread->quit();
        m_workerThread->wait();
        m_worker->deleteLater();
        m_workerThread->deleteLater();
    }

    m_workerThread = new QThread(this);
    m_worker = new VenvWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, [this, projectPath]()
    {
        m_worker->doCascadeSearch(projectPath);
    });

    connect(m_worker, &VenvWorker::searchFinished, this, [this, projectPath](bool success, const QString &path, const QStringList &pkgs, const QString &err) {
        if (success) {
            m_currentPythonPath = path;
            m_cachedPackages = pkgs;

            // 1. АППАРАТНО СОБИРАЕМ ИЗОЛИРОВАННУЮ СРЕДУ ДЛЯ ПРОЕКТА
            buildIsolatedEnvironment();

            // 2. ЗАПИСЫВАЕМ ПУТЬ В СИСТЕМНЫЙ КОНФИГ IDE
            saveEnvToConfig(projectPath, path);

            emit venvConnectedSuccessfully(pkgs);
        } else {
            emit venvNotFoundOrCorrupted(err);
        }
        m_workerThread->quit();
    });

    m_workerThread->start();
}

void VenvWorker::doCascadeSearch(const QString &projectPath) {
    Q_UNUSED(projectPath); // Нам больше не важен путь проекта для поиска venv!

    QStringList candidates;

    // =========================================================================
    // ЖЕСТКИЙ ПРИОРИТЕТ: ИЩЕМ ВНЕШНИЙ VENV В ВАШЕЙ ДОМАШНЕЙ ДИРЕКТОРblock LINUX
    // =========================================================================
    candidates << "/home/elf/venv/bin/python";

    // Запасные варианты на случай, если venv перенесут
    candidates << "/home/elf/pyTorch-Studio/venv/bin/python";
    candidates << "python3" << "python";

    QString workingPythonExe = "";
    QStringList packages;

    for (const QString &pythonPath : std::as_const(candidates)) {
        if (pythonPath.contains("/")) {
            if (!QFile::exists(pythonPath)) continue;
        }

        workingPythonExe = pythonPath;

        // Сканируем сайт-пакеты внешнего venv на наличие PyTorch
        QFileInfo pyInfo(pythonPath);
        QDir sitePackages;

        if (pyInfo.isRelative()) {
            QDir sysLib("/usr/lib/python3/dist-packages/torch");
            if (sysLib.exists()) sitePackages = sysLib;
        } else {
            QDir venvDir = pyInfo.absoluteDir();
            venvDir.cdUp(); // Поднимаемся из bin/ в корень /home/elf/venv

            QDir libDir(venvDir.absolutePath() + "/lib");
            QStringList pythonDirs = libDir.entryList(QStringList() << "python3.*", QDir::Dirs);
            if (!pythonDirs.isEmpty()) {
                libDir.cd(pythonDirs.first());
                libDir.cd("site-packages/torch");
                sitePackages = libDir;
            }
        }

        if (sitePackages.exists()) {
            packages << "torch (Библиотека обнаружена)";
        } else {
            packages << "чистый venv (PyTorch не установлен)";
        }
        break;
    }

    if (!workingPythonExe.isEmpty()) {
        qDebug() << "[WORKER SUCCESS] Внешний venv успешно идентифицирован:" << workingPythonExe;
        emit searchFinished(true, workingPythonExe, packages, "");
    } else {
        emit searchFinished(false, "", QStringList(), "Внешний интерпретатор Python не найден.");
    }
}

void PythonEnvManager::saveEnvToConfig(const QString &projectPath, const QString &pythonPath)
{
    Q_UNUSED(projectPath);

    // =========================================================================
    // ЕДИНЫЙ СТАНДАРТ: ЗАПИСЬ НАПРЯМУЮ В PYSTUDIO.CONF
    // =========================================================================
    QSettings settings("/home/elf/.config/PyTorchStudio/pystudio.conf", QSettings::IniFormat);

    // Сохраняем глобальный путь к вашему внешнему venv в домашней директории
    settings.setValue("GlobalEnvironment/external_venv_path", pythonPath.trimmed());
    settings.sync();

    qInfo() << "[CONFIG_SUCCESS] Путь к внешнему venv успешно зафиксирован в pystudio.conf:" << pythonPath;
}


void PythonEnvManager::buildIsolatedEnvironment() {
    m_isolatedEnv = QProcessEnvironment::systemEnvironment();

    if (m_currentPythonPath.isEmpty()) return;

    // Извлекаем директорию venv/bin
    QString venvBinDir = QFileInfo(m_currentPythonPath).absoluteDir().absolutePath();
    QString oldPath = m_isolatedEnv.value("PATH");

    // Внедряем venv/bin на первое место в PATH подпроцессов
    m_isolatedEnv.insert("PATH", venvBinDir + ":" + oldPath);

    // Указываем корень виртуального окружения
    m_isolatedEnv.insert("VIRTUAL_ENV", QDir(venvBinDir).filePath(".."));

    // Запрещаем Python заглядывать в глобальные системные папки ОС Linux
    m_isolatedEnv.remove("PYTHONHOME");

    // Прописываем PYTHONPATH на корень проекта для беспрепятственного импорта локальных модулей
    QFileInfo pyInfo(m_currentPythonPath);
    QDir projectDir = pyInfo.absoluteDir();
    projectDir.cdUp(); projectDir.cdUp(); // Поднимаемся из bin/ и venv/ в корень
    m_isolatedEnv.insert("PYTHONPATH", projectDir.absolutePath());

    // Отключаем буферизацию Python-вывода для мгновенного вливания логов в консоль logEdit [0:1.367]
    m_isolatedEnv.insert("PYTHONUNBUFFERED", "1");
}

// Глобальный метод изоляции среды для QProcess
QProcessEnvironment PythonEnvManager::getIsolatedEnvironment() const
{
    // Берем текущую чистую среду операционной системы
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    if (m_currentPythonPath.isEmpty()) return env;

    // Извлекаем директорию /home/elf/venv/bin
    QString venvBinDir = QFileInfo(m_currentPythonPath).absoluteDir().absolutePath();
    QString oldPath = env.value("PATH");

    // Внедряем внешний venv/bin в самый левый край PATH — это заблокирует системный Python!
    env.insert("PATH", venvBinDir + ":" + oldPath);

    // Прописываем корневую точку виртуального окружения
    env.insert("VIRTUAL_ENV", QDir(venvBinDir).filePath(".."));

    // Намертво вырезаем PYTHONHOME, чтобы подпроцессы не путали библиотеки
    env.remove("PYTHONHOME");

    // Разрешаем моментальный, небуферизированный вывод логов PyTorch
    env.insert("PYTHONUNBUFFERED", "1");

    return env;
}
