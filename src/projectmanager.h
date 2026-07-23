#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QRegularExpression>

class ProjectManager : public QObject
{
    Q_OBJECT
public:
    explicit ProjectManager(QObject *parent = nullptr);

    // =========================================================================
    // ТОЧНАЯ ДЕКЛАРАЦИЯ СИГНАТУРЫ ФУНКЦИИ ОБРАБОТКИ КЛЮЧЕЙ
    // =========================================================================
    QString processProjectKey(const QString &inputPath, bool &isArchive);

    // Управление историей сессий pystudio.conf
    void addProjectToRecentList(const QString &projectPath);
    QStringList getRecentProjects();

private:
    QString getSafeDefaultProjectsDir();
    bool unarchiveProject(const QString &archivePath, const QString &targetDir);
};

#endif // PROJECTMANAGER_H
