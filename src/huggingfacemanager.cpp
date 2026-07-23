#include "huggingfacemanager.h"
#include <QDir>
#include <QDebug>

HuggingFaceManager::HuggingFaceManager(QObject *parent)
    : QObject(parent), m_process(new QProcess(this)), m_isBusy(false)
{
    connect(m_process, &QProcess::readyReadStandardError, this, &HuggingFaceManager::handleReadyReadStandardError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &HuggingFaceManager::handleReadyReadStandardOutput);
    connect(m_process, &QProcess::errorOccurred, this, &HuggingFaceManager::handleProcessError);
    connect(m_process, &QProcess::finished, this, &HuggingFaceManager::handleProcessFinished);
}

HuggingFaceManager::~HuggingFaceManager()
{
    cancelOperation();
}

QProcessEnvironment HuggingFaceManager::createIsolatedEnvironment(const QString &projectFolderPath)
{
    QDir dir(projectFolderPath);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // ЖЕСТКАЯ ИЗОЛЯЦИЯ: Заставляем утилиту качать кэш строго в локальную hf_hub текущего проекта
    env.insert("HF_HOME", dir.absoluteFilePath("hf_hub"));

    // Отключаем интерактивные вопросы в консоли, так как процесс работает вheadless-режиме
    env.insert("HF_HUB_DISABLE_TELEMETRY", "1");
    env.insert("PYTHONUNBUFFERED", "1"); // Вывод прогресс-баров без буферизации
    return env;
}

bool HuggingFaceManager::downloadAsset(const QString &projectFolderPath, const QString &repoId, const QString &filename)
{
    if (m_isBusy) {
        emit hfLogReceived("⚠️ [🤗 HF] Ошибка: Менеджер занят выполнением другой операции.\n");
        return false;
    }

    m_projectPath = projectFolderPath;
    m_isBusy = true;

    m_process->setProcessEnvironment(createIsolatedEnvironment(m_projectPath));
    m_process->setWorkingDirectory(m_projectPath);

    QStringList arguments;
    arguments << "download";
    arguments << repoId;

    if (!filename.isEmpty()) {
        arguments << filename; // Если нужно скачать один конкретный файл весов, а не весь репозиторий
    }

    emit hfLogReceived(QString("🤗 <b>[HF] Начинаю асинхронную загрузку репозитория '%1'...</b><br>").arg(repoId));

#if defined(Q_OS_WIN)
    m_process->start("huggingface-cli.exe", arguments);
#else
    m_process->start("huggingface-cli", arguments); // Стандарт для Linux/Arch
#endif

    if (!m_process->waitForStarted(3000)) {
        m_isBusy = false;
        emit operationFinished(false, "Не удалось запустить системную утилиту huggingface-cli.");
        return false;
    }

    return true;
}

bool HuggingFaceManager::loginWithToken(const QString &projectFolderPath, const QString &token)
{
    if (m_isBusy) return false;

    m_projectPath = projectFolderPath;
    m_isBusy = true;

    m_process->setProcessEnvironment(createIsolatedEnvironment(m_projectPath));
    m_process->setWorkingDirectory(m_projectPath);

    QStringList arguments;
    arguments << "login" << "--token" << token;

    emit hfLogReceived("🤗 <b>[HF] Отправка токена авторизации на сервер Hugging Face...</b><br>");

#if defined(Q_OS_WIN)
    m_process->start("huggingface-cli.exe", arguments);
#else
    m_process->start("huggingface-cli", arguments);
#endif

    if (!m_process->waitForStarted(3000)) {
        m_isBusy = false;
        emit operationFinished(false, "Не удалось запустить процедуру логина Hugging Face.");
        return false;
    }

    return true;
}

void HuggingFaceManager::cancelOperation()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        emit hfLogReceived("<br>🛑 <b>[HF] Принудительная отмена операции загрузки...</b><br>");
        m_process->kill();
        m_process->waitForFinished();
        m_isBusy = false;
    }
}

bool HuggingFaceManager::isBusy() const { return m_isBusy; }

void HuggingFaceManager::handleReadyReadStandardError()
{
    // Индикатор прогресса скачивания утилиты huggingface-cli транслируется в поток Standard Error
    QString output = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    if (!output.isEmpty()) {
        emit hfLogReceived("[🤗 HF]: " + output + "\n");
    }
}

void HuggingFaceManager::handleReadyReadStandardOutput()
{
    QString output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    if (!output.isEmpty()) {
        emit hfLogReceived("[🤗 HF Out]: " + output + "\n");
    }
}

void HuggingFaceManager::handleProcessError(QProcess::ProcessError error)
{
    m_isBusy = false;
    QString msg = QString("Ошибка подсистемы Hugging Face: %1 (%2)").arg(error).arg(m_process->errorString());
    emit operationFinished(false, msg);
}

void HuggingFaceManager::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_isBusy = false;
    bool success = (exitCode == 0 && exitStatus == QProcess::NormalExit);

    if (success) {
        emit hfLogReceived("<br>✅ <b>[HF] Операция успешно завершена!</b><br>");
    } else {
        emit hfLogReceived(QString("<br><font color='red'>❌ <b>[HF] Процесс завершился ошибкой OS. Код: %1</b></font><br>").arg(exitCode));
    }

    emit operationFinished(success, success ? "Успешно" : "Сбой системного процесса");
}
