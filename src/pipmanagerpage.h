#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>

class Neuro_programm;

class PipManagerPage : public QWidget {
    Q_OBJECT

public:
    // Передаем путь к venv и путь к файлу requirements.txt проекта
    explicit PipManagerPage(const QString &venvDir, const QString &requirementsPath, QWidget *parent = nullptr);
    ~PipManagerPage() override = default;
    bool isPackageInstalled(const QString &packageName);
    void loadPipData(QString pkgName = "");
    void highlightAndScrollToPackage(const QString &packageName);

signals:
    void dataLoaded();

private slots:
    void onPipListFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onPyPiReplyFinished(QNetworkReply* reply);
    void showContextMenu(const QPoint &pos);

private:
    QString m_venvDir;
    QString m_requirementsPath;
    QString m_pythonExe;
    QString m_packageToHighlight;
    QTableWidget *m_table;
    QPushButton *m_refreshBtn;
    QVBoxLayout *m_layout;

    QProcess *m_pipProcess;
    QNetworkAccessManager *m_networkManager;

    QSet<QString> m_requiredPackages; // Пакеты, описанные в requirements.txt
    int m_pendingRequests;

    void parseRequirementsFile();
    void updateStatusCell(int row, const QString &installedVer, const QString &latestVer, const QString &pkgName);
};
