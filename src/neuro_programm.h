#ifndef NEURO_PROGRAMM_H
#define NEURO_PROGRAMM_H
#include "settings.h"
//#include "aiprojectmodel.h"
#include "about_program.h"
#include "projectrootproxymodel.h"
#include "start_progect.h"
#include "panel_other.h"
#include "search.h"
#include "elidedlabel.h"
#include "ai_panel.h"
#include "pythonenvmanager.h"
#include "pipmanagerpage.h"
#include "jupytermanager.h"
#include "tensorboardmanager.h"
#include "huggingfacemanager.h"
#include "debugmanager.h"
#include "projectmanager.h"
#include "documentmanager.h"
#include "prog_stm.h"
//#include "stacktablehandler.h"
#include "variablestablehandler.h"
#include "savedata.h"
#include "localaimanager.h"
#include "aipromptwidget.h"
#include "sessiontablewidget.h"
#include "sessiondetailswidget.h"

#include <QMainWindow>
#include <QCompleter>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineSeries>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QSplitter>
#include <QStringListModel>
#include <QTcpSocket>
#include <QIODevice>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QPointer>
#include <QTextBrowser>
#include <QWebEngineView>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QElapsedTimer>

class JupyterClient;
class JupyterManager;
class AI_panel;

QT_BEGIN_NAMESPACE
namespace Ui {
class Neuro_programm;
}
QT_END_NAMESPACE

class Neuro_programm : public QMainWindow
{
    Q_OBJECT

    friend class panel_other;

public:
    explicit Neuro_programm(const QString &startupPath = "", QWidget *parent = nullptr);
    ~Neuro_programm() override;
    Ui::Neuro_programm *ui;
    class AI_panel *aiPanel = nullptr;

    void sync();
    void setIDEInStartMode(bool isStartMode);
    void open_project(const QString &path = "");
    void forceOpenConsoleWithError(const QString &errorMessage);
    void processEnvironmentAndSync(const QString &projectPath, const QString &architecture = "AUTO");
    QString calculateFileMd5(const QString &filePath);
    void syncVenvToRequirements();
    QString getCurrentProjectPath() const { return this->currentOpenProjectPath; }
    void updateStatusLogText(const QString &text, const QString &colorHtml = QStringLiteral("#eff0f1")) {
        // Проверяем живой указатель на метку, сохраненную на страницах 2 и 24 вашего лога
        if (this->statusLogLabel) {
            this->statusLogLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(colorHtml));
            this->statusLogLabel->setFullText(text); // Используем ваш кастомный метод setFullText
        }
    }
    void updateProjectsListFromSettings();
    QElapsedTimer m_aiGenerationTimer;
    static Neuro_programm* self;
    QString getCurrentOpenFilePath() const;
    void showFloatingDocumentation(const QString &htmlContent);
    void sendLspRequest(const QString &method, const QJsonObject &params, int id = 0);
    QStringList temporaryOpenFilesBackup;
    QProcess* getLspProcess() const { return lspProcess; }
    int globalLspDocVersion = 1;
    ElidedLabel *statusLogLabel = nullptr;
    AiPromptWidget *m_activePromptWidget = nullptr;
    int lspRequestId = 0;
    void applyGlobalFonts();
    struct LspErrorData {
        int line;
        int startChar;
        int endChar;
        bool isError;
        QString message;
        QString code;
    };
    enum PageIndex {
        PageLogs = 0,         // Обычный текстовый вывод
        PagePipInstall = 1,   // Консоль установки пакетов
        PageTensorBoard = 2,  // Графики
        PagePipTable = 3      // Таблица установленных пакетов (Ваш QTableWidget)
    };
    static QList<LspErrorData> globalLspErrors;
    QProcess *lspProcess = nullptr;
    QProcess *tensorboardProcess = nullptr;
    panel_other *panelOther;
    QPointer<QTextBrowser> m_docWindow;
    void updateCustomTitle(const QString &fileName);
    void runPipUpgradeProcess(const QString &packageName);
    void runPipUninstallProcess(const QString &packageName);
    void onDetectDevice();
    void onSelectFirmwareFile();
    void onEraseFlash();
    void onWrightFlash();
    void onReadFlash();
    void startFlashWritingProcess();

public slots:
    void updateJediStatusText(const QString &message, bool isError);
    void updateJediStatusTextFromLsp(int errorCount);
    void refreshProblemsTableView();

signals:
    void signalSendChunkToConsole(const QString &text);
    void completionDataReceived(const QStringList &completions);
    void firmwareFlashSuccess();
    void aiCodeGenerationRequested(int cursorPosition, const QString &promptText, const QString &fullCodeContext);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

protected slots:
    void new_progect();
    void openNewFileInEditor(const QString &absoluteFilePath);
    void open_settings();
    void onOpenProjectMenuTriggered();
    void onSaveProjectMenuTriggered();
    void open_about_program();
    void close_program();
    void saveProjectAs();
    void btnStartDebug_clicked();
    void onFindNext();
    void onFindPrev();
    void onSelectAll();
    void loadProjectFromSettingsList(QListWidgetItem *item);
    bool unarchiveProject(const QString &archivePath, const QString &targetExtractDir);

private:
    bool hasPythonCrashOccurred;
    bool bootstrapProjectStructure(const QString &rootPath);
    void detectCudaDevices();
    void sendSystemNotification(const QString &title, const QString &text);
    void initProjectTreeModel(QString path);
    void updateRecentProjectActions();
    void addProjectToRecent(const QString &projectPath);
    void initLspServer();
    QWidget *activeCompletionPopup = nullptr;
    void sendInitialWelcomeRequest();
    int codeBlockCounter = 0;
    int responseCounter = 0;
    QMap<QString, QString> aiResponsesMap;
    QMap<QString, QString> codeBlocksMap;
    void onChatAnchorClicked(const QUrl &link);
    QString parseMarkdownCodeBlocks(const QString &rawText);
    void handlePythonErrors();
    void onQuickActionTriggered(QListWidgetItem *item);
    QPushButton *btnStatusAI = nullptr;
    void applyThemeColors(bool isDarkTheme);
    QString getSafeSaveFolderPath();
    void saveSettings();
    void updateCursorPositionIndicator();
    void updateBottomPanelGeometry();
    void initializeEnvironmentOnStartup();
    void showVenvEmergencyDialog(const QString &reason);
    void load_progect(const QString &projectPath);
    bool createProjectPassport(const QString &projectName, const QString &projectFolderPath, bool useGpuArchitecture);
    void setupInstallProcessConnections();
    void initTensorBoardUi();
    void processStartupPath(const QString &path);
    QString getChipNameById(uint32_t chipId);
    void setDebugButtonsEnabled(bool enabled);
    bool showSaveConfirmationDialog();
    void injectFinalMetricsToVariableTree(const QString &name, const QString &value, const QString &type);
    void jumpToCodeLine(const QString &filePath, int lineNumber);
    void validatePythonSyntax(const QString &filePath);
    void setupSessionTableConnections();
    QTimer *linterDebounceTimer = nullptr;

private slots:
    void onFileDoubleClicked(const QModelIndex &index);
    void onCloseCurrentFileClicked();
    void onOpenFileListItemDoubleClicked(QListWidgetItem *item);
    //void onStopTrainingClicked();
    void readTrainingOutput();
    void trainingFinished(int exitCode);
    void save_project_config();
    void openRecentProject();
    void saveCurrentActiveFile();
    void saveAllProjectChanges();
    void onCurrentFileTextChanged();
    void onCloseProjectClicked();
    void readLspResponse();
    void showCompletionMenuInGuiThread(const QStringList &completions);
    void triggerEditAction();
    void onInstallSinglePackageTriggered();
    void install_from_requirements();
    void onTorchCacheProcessFinished();
    void showTreeViewContextMenu(const QPoint &pos);
    void onCreateFileRequested(const QString &parentPath);
    void onCreateFolderRequested(const QString &parentPath);
    void onExecuteScriptRequested(const QString &scriptPath);
    void onGitStatusRequested();
    void onGitCommitRequested();
    void onGitPushRequested();
    void updateCodeSearch();
    void onReplaceCurrent();
    void onReplaceAndFindNext();
    void onReplaceAll();
    void action_uninstall_package_triggered();
    void action_upgrade_package_triggered();
    void action_upgrade_all_packages_triggered();
    void action_install_from_requirements_triggered();
    void action_freeze_requirements_triggered();
    void saveCurrentProjectChanges();
    void saveProjectAsArchive();
    void open_STM();
    void open_STM_work();
    void openAiPromptBox();
    void onPromptSubmitted(const QString &promptText);

private:
    QMetaObject::Connection jupyterLogConnection;
    QMetaObject::Connection jupyterFinishConnection;
    QMetaObject::Connection jupyterReadyConnection;
    SessionDetailsWidget *detailsDashboard;
    SessionTableWidget *sessionTable{nullptr};
    AI_panel *m_aiPanel = nullptr;
    bool m_isOpeningFile = false;
    bool isAiProcessing = false;
    LocalAiManager* m_aiManager;
    QMenu *toolsMenu;
    QMenuBar *customMenuBar;
    QAction *actControlPanel = nullptr;
    QAction *actDebug = nullptr;
    QAction *actProject = nullptr;
    QAction *actResume = nullptr;
    QAction *actStepOver = nullptr;
    QAction *actStepInto = nullptr;
    QAction *actStopDebug = nullptr;
    QAction *actTrainConfig = nullptr;
    VariablesTableHandler *m_variablesHandler = nullptr;
    int m_previousPageIndex = 0;
    QAction *actSTM;
    QAction *actSTM_work;
    QAction *EraseFlash;
    QAction *WrightFlash;
    QString m_firmwarePath;
    DocumentManager *docMgr;
    ProjectManager *projectMgr;
    DebugManager *pyDebugger;
    HuggingFaceManager *hfManager;
    QWebEngineView *m_tensorWebView;
    TensorBoardManager *tensorBoardServer;
    JupyterManager *jupyterServer;
    JupyterClient  *jupyterClient;
    QProcess *m_installProcess = nullptr;
    PythonEnvManager *envManager;
    PipManagerPage *m_pipPage = nullptr;
    Start_progect *rsc;
    QString m_pendingAutoloadFile;
    Settings    *rsc2;
    About_program *rsc3;
    QWidget *rsc4;
    Savedata *rsc5;
    Search *search;
    QFileSystemModel *projectModel = nullptr;
    ProjectRootProxyModel *projectProxyModel = nullptr;
    QSplitter   *mainVerticalSplitter;
    QPushButton *btnTerminal;
    QPushButton *btnSearch;
    QPushButton *btnLogs;
    QPushButton *btnTogglePip;
    QPushButton *btnAIChat;
    QPushButton *btnStartDebug;
    QProcess *trainingProcess;
    QChart      *lossChart;
    QLineSeries *lossSeries;
    int          currentEpochCounter;
    QString currentOpenProjectPath;
    void sendChatMessageToAI();
    static const int MaxRecentFiles = 5;
    QMenu *recentProjectsMenu;
    QAction *recentProjectActions[MaxRecentFiles];
    QCompleter *codeCompleter = nullptr;
    QStringListModel *completerModel = nullptr;
    QString venvPythonBinary;
    QNetworkAccessManager *networkManager;
    QWidget *titleBarWidget = nullptr;
    QLabel *titleLabel = nullptr;
    QPushButton *btnMinimize = nullptr;
    QPushButton *btnMaximize = nullptr;
    QPushButton *btnExit = nullptr;
    QPoint m_dragPosition;
    QWidget *topHeaderPanel = nullptr;
    QLabel *topTitleLabel = nullptr;
    QPushButton *topBtnInfo = nullptr;
    QPushButton *topBtnStatus = nullptr;
    QPushButton *topBtnSettings = nullptr;
    QWidget *statusSpacer = nullptr;
    QByteArray m_lspAccumulatedBuffer;
    QWidget *leftSideBarContainer = nullptr;
    CodeEditor *currentEditor;
    bool isDocWindowActive = false;
    bool m_dragging = false;
    bool m_isDragging = false;
    class QSpacerItem *leftPaddingSpacer = nullptr;
    void updateWidget3Padding();
    QString currentFilePath;
    QTimer *monitorTimer;
    QProcess *debuggedScriptProcess;
    QAction *actTensor = nullptr;
    QAction *actPip = nullptr;
    QAction *actSearch = nullptr;
    QAction *actStartTrain = nullptr;
    QAction *actStop;
    QAction *actStepOut;
    QPlainTextEdit *logEdit;
    void updateTabName();
    void setFileModifiedState(CodeEditor *editor, bool modified);
    bool archiveProject(const QString &sourceDir, const QString &outputSavePath);
    void saveProjectParameters(const QString &tmpDir);
    void loadProjectParameters(const QString &tmpDir);
    void sendLspDidOpenForFile(const QString &filePath, const QString &fileContent);
    void checkAndCreateVenvAsync(const QString &projectPath, bool isFreshExtract = false);
    void installPackagesFromRequirements(const QString &workingDir, const QString &pythonPath, const QString &reqPath);
    void startTensorBoard(const QString &logDir);
    void updateAiStackCache();
    QString cachedOllamaVersion;
    QProcess *torchCacheProc = nullptr;
    CodeEditor* getCurrentEditor();
    void highlightCurrentMatch(QTextCursor symbolCursor);
    void updateFunctionNavigator(CodeEditor *editor);
    void on_btnSidebarTerminal_clicked();
    void action_install_package_triggered();
    bool createServicesConfig(const QString &projectName, const QString &projectFolderPath);
    bool createDefaultTrainNotebook(const QString &projectFolderPath);
    void closeStlink();
    void edit_intfce();
    void add_vars_debug();
    QStandardItemModel *m_varModel = nullptr;
    void setupDebugInterface();
    QStandardItemModel *m_sourcesModel = nullptr;
    void createMenus();
    void showPreferences();
    void insertGeneratedCodeIntoEditor(const QString &generatedCode);
    QString generateSessionId(const QString &modelTag, const QString &optimizer, const QString &device, const QString &shortComment);
};
#endif // NEURO_PROGRAMM_H
