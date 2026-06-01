#ifndef PANEL_OTHER_H
#define PANEL_OTHER_H

#include <QProcess>
#include <QWidget>

namespace Ui {
class Panel_other;
}

class Panel_other : public QWidget
{
    Q_OBJECT

public:
    explicit Panel_other(QWidget *parent = nullptr);
    ~Panel_other();
    void startVenvInstallation(const QString &projectPath, const QString &archType);
    void setTerminalPageActive();
    void setSearchPageActive();
    void setLogsPageActive();
    void togglePipPanel(bool visible);
    void setCurrentProjectPath(const QString &path);
    void appendLiveLogText(const QString &text);


signals:
    void pipPanelClosed();
    void signalSendChunkToConsole(const QString &text);

private slots:
    void executeCustomPipCommand(const QString &packageName);

private:
    Ui::Panel_other *ui;
    QProcess *process;
    int progressStartPosition; // Позиция начала блока прогресс-бара
    bool isDownloading;
    QString currentProjectPath;
};

#endif // PANEL_OTHER_H
