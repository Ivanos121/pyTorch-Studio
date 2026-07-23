#include "projectmanager.h"
#include <QProcess>
#include <QDebug>

ProjectManager::ProjectManager(QObject *parent) : QObject(parent)
{
    // Конструктор метасистемы Qt6
}

// =========================================================================
// РЕАЛИЗАЦИЯ МЕТОДА ОБРАБОТКИ КЛЮЧЕЙ С ОТЛАЖЕННОЙ СМАРТ-ЛОГИКОЙ
// =========================================================================
QString ProjectManager::processProjectKey(const QString &inputPath, bool &isArchive)
{
    isArchive = false;
    QFileInfo fileInfo(inputPath);
    if (!fileInfo.exists()) return QString();

    QString fileName = fileInfo.fileName().toLower();
    QString suffix = fileInfo.suffix().toLower();

    // СЦЕНАРИЙ 1: КЛЮЧ-МАНИФЕСТ (Локальный режим открытия папки)
    if (fileName == "passport.pystudio.json" || fileName.endsWith(".pystudio.json")) {
        qInfo() << "[PROJ_MGR] Обнаружен локальный манифест паспорта. Путь валиден.";
        return fileInfo.absolutePath(); // Возвращаем родительскую рабочую директорию
    }

    // СЦЕНАРИЙ 2: КЛЮЧ-АРХИВ (Портативный режим импорта tar -xjf на новой машине)
    if (suffix == "pystudio") {
        isArchive = true;
        qInfo() << "[PROJ_MGR] Обнаружен переносимый бинарный архив. Запускаю распаковку...";

        QString defaultDir = getSafeDefaultProjectsDir();
        QString targetExtractDir = defaultDir + "/" + fileInfo.baseName();

        // Защита от перезаписи: инкрементируем имя папки, если проект с таким именем уже есть
        int counter = 1;
        QString originalDir = targetExtractDir;
        while (QDir(targetExtractDir).exists()) {
            targetExtractDir = originalDir + "_" + QString::number(counter);
            counter++;
        }

        QDir().mkpath(targetExtractDir);

        if (unarchiveProject(inputPath, targetExtractDir)) {
            return targetExtractDir;
        } else {
            QDir(targetExtractDir).removeRecursively(); // Зачищаем мусор при сбое
        }
    }

    return QString(); // Фолбэк, если формат ключа неверен
}

void ProjectManager::addProjectToRecentList(const QString &projectPath)
{
    if (projectPath.isEmpty()) return;
    QString configAbsolutePath = QDir::homePath() + "/.config/PyTorchStudio/pystudio.conf";
    QSettings settings(configAbsolutePath, QSettings::IniFormat);

    settings.beginGroup("Main");
    QString rawList = settings.value("recentProjectList", "").toString().trimmed();
    QStringList projectList = rawList.split(QRegularExpression("[,;]"), Qt::SkipEmptyParts);

    for (int i = 0; i < projectList.size(); ++i) projectList[i] = projectList[i].trimmed();

    projectList.removeAll(projectPath);
    projectList.prepend(projectPath); // Поднимаем в самый верх (Топ-1) истории

    while (projectList.size() > 5) projectList.removeLast(); // Ограничиваем топ-5 недавних

    settings.setValue("recentProjectList", projectList.join(";"));
    settings.endGroup();
    settings.sync();
}

QStringList ProjectManager::getRecentProjects()
{
    QString configAbsolutePath = QDir::homePath() + "/.config/PyTorchStudio/pystudio.conf";
    QSettings settings(configAbsolutePath, QSettings::IniFormat);

    settings.beginGroup("Main");
    QString rawList = settings.value("recentProjectList", "").toString().trimmed();
    settings.endGroup();
    return rawList.split(QRegularExpression("[,;]"), Qt::SkipEmptyParts);
}

QString ProjectManager::getSafeDefaultProjectsDir()
{
    QString path = QDir::homePath() + "/projects";
    QDir().mkpath(path);
    return path;
}

bool ProjectManager::unarchiveProject(const QString &archivePath, const QString &targetDir)
{
    QProcess tar;
    tar.setWorkingDirectory(targetDir);
    tar.start("tar", QStringList() << "-xjf" << archivePath << "-C" << targetDir);
    return tar.waitForFinished(5000) && tar.exitCode() == 0;
}
