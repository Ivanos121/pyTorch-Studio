#ifndef NEURO_PROGRAMM_H
#define NEURO_PROGRAMM_H

#include <QChart>
#include <QCompleter>
#include <QFileSystemModel>
#include <QLineSeries>
#include <QListWidget>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QSplitter>
#include <QStringListModel>
#include "start_progect.h"
#include "panel_other.h"
#include <QTcpSocket>
#include "settings.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Neuro_programm;
}
QT_END_NAMESPACE

class Neuro_programm : public QMainWindow
{
    Q_OBJECT

public:
    explicit Neuro_programm(QWidget *parent = nullptr);
    ~Neuro_programm() override;
    Ui::Neuro_programm *ui;

    void sync();
    void open_project();
    void forceOpenConsoleWithError(const QString &errorMessage);
    static Neuro_programm* self;
    QString getCurrentOpenFilePath() const;
    void sendLspRequest(const QString &method, QJsonObject params);
    QStringList temporaryOpenFilesBackup;
    QProcess* getLspProcess() const { return lspProcess; }
    int globalLspDocVersion = 1;
    int lspRequestId = 0;

signals:
    void signalSendChunkToConsole(const QString &text);
    void completionDataReceived(const QStringList &completions);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

protected slots:
    void new_progect();

    void openNewFileInEditor(const QString &absoluteFilePath);
    void open_settings();
private:
    bool bootstrapProjectStructure(const QString &rootPath);
    void detectCudaDevices();
    void sendSystemNotification(const QString &title, const QString &text);
    void initProjectTreeModel(const QString &path);
    void initLossChart();
    void updateRecentProjectActions();   // Метод перерисовки списка в меню
    void addProjectToRecent(const QString &projectPath); // Метод добавления нового пути
    void initLspServer();
    QWidget *activeCompletionPopup = nullptr;
    void sendInitialWelcomeRequest();
    int codeBlockCounter = 0;
    int responseCounter = 0;
    QMap<QString, QString> aiResponsesMap;
    QMap<QString, QString> codeBlocksMap; // Хранилище: ID блока -> Чистый код
    void onChatAnchorClicked(const QUrl &link);
    QString parseMarkdownCodeBlocks(const QString &rawText);
    void handlePythonErrors();
    void onQuickActionTriggered(QListWidgetItem *item);
    QPushButton *btnStatusAI = nullptr;



private slots:
    void onFileDoubleClicked(const QModelIndex &index);
    void onCloseCurrentFileClicked();
    void onOpenFileListItemDoubleClicked(QListWidgetItem *item);
    void onStartTrainingClicked();   // Нажатие на "Начать обучение"
    void onStopTrainingClicked();    // Нажатие на "Остановить"
    void readTrainingOutput();       // Асинхронное построчное чтение логов лосса
    void trainingFinished(int exitCode);
    void save_project_config();
    void openRecentProject(); // Слот, который срабатывает при клике на недавний проект
    void saveCurrentActiveFile(); // Сохранить текущий открытый файл (Ctrl + S)
    void saveAllProjectChanges();
    void onCurrentFileTextChanged();
    void onCloseProjectClicked();
    void readLspResponse();
    void showCompletionMenuInGuiThread(const QStringList &completions);

private:
    Start_progect *rsc;
    Settings    *rsc2;
    Panel_other *panelOther;
    QSplitter   *mainVerticalSplitter;
    QPushButton *btnTerminal;
    QPushButton *btnSearch;
    QPushButton *btnLogs;
    QPushButton *btnTogglePip;
    QPushButton *btnAIChat;
    QFileSystemModel *projectModel;
    QProcess *trainingProcess;
    QChart      *lossChart;
    QLineSeries *lossSeries;
    int          currentEpochCounter;
    QString currentOpenProjectPath;
    void sendChatMessageToAI();
    static const int MaxRecentFiles = 5;
    QMenu *recentProjectsMenu;           // Подменю "Открыть недавние"
    QAction *recentProjectActions[MaxRecentFiles];
    QProcess *lspProcess = nullptr;
    QCompleter *codeCompleter = nullptr;
    QStringListModel *completerModel = nullptr;
    QString venvPythonBinary;
    QNetworkAccessManager *networkManager;



};
#endif // NEURO_PROGRAMM_H
