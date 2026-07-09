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

void PythonEnvManager::startBackgroundCheck(const QString &projectPath) {
    // Если предыдущий поток еще работает — вежливо тушим его
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        m_worker->deleteLater();
        m_workerThread->deleteLater();
    }

    m_workerThread = new QThread(this);
    m_worker = new VenvWorker();
    m_worker->moveToThread(m_workerThread);

    // Связываем запуск потока с выполнением тяжелого поиска
    connect(m_workerThread, &QThread::started, m_worker, [this, projectPath]() {
        m_worker->doCascadeSearch(projectPath);
    });

    // Принимаем результат из фона обратно в менеджер
    connect(m_worker, &VenvWorker::searchFinished, this, [this](bool success, const QString &path, const QStringList &pkgs, const QString &err) {
        if (success) {
            m_currentPythonPath = path;
            m_cachedPackages = pkgs;
            emit venvConnectedSuccessfully(pkgs);
        } else {
            emit venvNotFoundOrCorrupted(err);
        }

        // Корректно останавливаем поток после завершения задачи
        m_workerThread->quit();
    });

    m_workerThread->start(); // Поехали в фоне!
}

void VenvWorker::doCascadeSearch(const QString &projectPath) {
    if (projectPath.isEmpty()) {
        emit searchFinished(false, "", QStringList(), "Путь к проекту пуст.");
        return;
    }

    QStringList candidates;
#ifdef Q_OS_WIN
    candidates << projectPath + "/venv/Scripts/python.exe";
    candidates << "python.exe";
#else
    candidates << projectPath + "/venv/bin/python";
    candidates << "python3" << "python";
#endif

    QString workingPythonExe = "";
    QStringList packages;

    for (const QString &pythonPath : std::as_const(candidates)) {
        // Проверяем абсолютные пути на физическое существование самого Python на диске
        if (pythonPath.contains("/")) {
            if (!QFile::exists(pythonPath)) {
                continue; // Файла Python нет, идем к следующему кандидату
            }
        }

        // Если файл python/python.exe найден — окружение уже ЖИВОЕ и легитимное!
        workingPythonExe = pythonPath;

        // Теперь проверяем, установлен ли внутри конкретно torch
        QFileInfo pyInfo(pythonPath);
        QDir sitePackages;

        if (pyInfo.isRelative()) {
            // Для системного Python
            QDir sysLib("/usr/lib/python3/dist-packages/torch");
            if (sysLib.exists()) sitePackages = sysLib;
        } else {
            // Для локального venv
            QDir venvDir = pyInfo.absoluteDir();
            venvDir.cdUp();
#ifdef Q_OS_WIN
            sitePackages = QDir(venvDir.absolutePath() + "/Lib/site-packages/torch");
#else
            QDir libDir(venvDir.absolutePath() + "/lib");
            QStringList pythonDirs = libDir.entryList(QStringList() << "python3.*", QDir::Dirs);
            if (!pythonDirs.isEmpty()) {
                libDir.cd(pythonDirs.first());
                libDir.cd("site-packages/torch");
                sitePackages = libDir;
            }
#endif
        }

        if (sitePackages.exists()) {
            packages << "torch (Библиотека обнаружена)";
        } else {
            packages << "чистый venv (PyTorch не установлен)";
        }

        break; // Нашли рабочий интерпретатор Python, прерываем поиск кандидатов!
    }

    // ТРИГГЕР УСПЕХА: Главное, чтобы был найден сам рабочий Python!
    if (!workingPythonExe.isEmpty()) {
        qDebug() << "[WORKER] Окружение валидно. Интерпретатор найден:" << workingPythonExe;
        emit searchFinished(true, workingPythonExe, packages, "");
    } else {
        qWarning() << "[WORKER] Сбой: Ни один интерпретатор Python не найден.";
        emit searchFinished(false, "", QStringList(), "Критическая ошибка: Интерпретатор Python не найден на диске.");
    }
}




