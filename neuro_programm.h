#ifndef NEURO_PROGRAMM_H
#define NEURO_PROGRAMM_H

#include <QChart>
#include <QFileSystemModel>
#include <QLineSeries>
#include <QListWidget>
#include <QMainWindow>
#include <QSplitter>
#include "start_progect.h"
#include "panel_other.h"

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

signals:
    void signalSendChunkToConsole(const QString &text);

protected slots:
    void new_progect();

private:
    bool bootstrapProjectStructure(const QString &rootPath);
    void detectCudaDevices();
    void sendSystemNotification(const QString &title, const QString &text);
    void initProjectTreeModel(const QString &path);
    void initLossChart();
    void updateRecentProjectActions();   // Метод перерисовки списка в меню
    void addProjectToRecent(const QString &projectPath); // Метод добавления нового пути

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



private:
    Start_progect *rsc;
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
    QAction *recentProjectActions[MaxRecentFiles]; // Массив экшенов под каждый проект

};
#endif // NEURO_PROGRAMM_H
