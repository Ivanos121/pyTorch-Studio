#ifndef PANEL_OTHER_H
#define PANEL_OTHER_H

#include <QProcess>
#include <QTcpSocket>
#include <QWidget>

class REPLWidget;

namespace Ui {
class panel_other;
}

class panel_other : public QWidget
{
    Q_OBJECT

public:
    explicit panel_other(QWidget *parent = nullptr);
    ~panel_other();
    void startVenvInstallation(const QString &projectPath, const QString &archType);
    void setSearchPageActive();
    void setLogsPageActive();
    void togglePipPanel(bool visible);
    void setCurrentProjectPath(const QString &path);
    void appendLiveLogText(const QString &text);
    void setTerminalPageActive();
    void setPipPageActive();
    void setInstallProgressVisible(bool visible);
    void setInstallProgressValue(int value);
    void setInstallProgressRange(int min, int max);
    void forwardCodeToREPL(const QString &code);
    void updateProjectVenv(const QString &projectPath);
    enum PageIndex {
        PageTerminal = 0,    // Страница 1: Терминал (и консоль логов установки)
        PageSearchReplace = 1, // Страница 2: Поиск и замена в файлах
        PagePipTable = 2       // Страница 3: Таблица установленных пакетов python
    };
    void setActivePage(PageIndex page);
    void connectToDebugger();
    void appendLogText(const QString &text);
    void appendDebugLog(const QString &text);

    Ui::panel_other *ui;


signals:
    void pipPanelClosed();
    void signalSendChunkToConsole(const QString &text);
    void panelClosed();

protected slots:
    void btnClosePanel();
    void refreshPipList();

private slots:
    void executeCustomPipCommand(const QString &packageName);
    void readTerminalOutput();
    void sendTerminalCommand();
    void readDebugSocket();
    void sendDebugCommand();

private:
    QProcess *process;
    int progressStartPosition; // Позиция начала блока прогресс-бара
    bool isDownloading;
    QString currentProjectPath;
    REPLWidget *replBackend;
    QProcess *terminalProcess;
    int debugSeqCounter;
    QTcpSocket *debugSocket;

    void initSystemTerminal();
    void initDebugConsole();
};

#endif // PANEL_OTHER_H
