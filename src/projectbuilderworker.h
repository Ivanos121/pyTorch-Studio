#ifndef PROJECTBUILDERWORKER_H
#define PROJECTBUILDERWORKER_H

#include <QObject>
#include <QString>
#include <QStringList>

class ProjectBuilderWorker : public QObject
{
    Q_OBJECT
public:
    explicit ProjectBuilderWorker(const QString &projectPath,
                                  const QString &projectName,
                                  bool useGpu,
                                  bool useCustomReq,
                                  const QString &customReqPath,
                                  const QString &venvPath,
                                  QObject *parent = nullptr);

    QString m_venvPath;


public slots:
    void startBuildPipeline();

signals:
    void progressStepChanged(int step, const QString &statusText);
    void logOutputReceived(const QString &logText);
    void pipelineBuildFinished(bool success, const QString &message);

private:
    bool initializeGitRepository();
    bool createVirtualEnvironment(QString &outVenvPath);
    bool installMLOpsDependencies(const QString &venvPath);
    bool registerJupyterKernel(const QString &venvPath);

    bool runSystemCommand(const QString &program, const QStringList &arguments, const QString &workingDir);

    // Параметры проекта
    QString m_projectPath;
    QString m_projectName;
    bool m_useGpu;
    bool m_useCustomReq;
    QString m_customReqPath;
};

#endif // PROJECTBUILDERWORKER_H
