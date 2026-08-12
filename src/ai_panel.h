#ifndef AI_PANEL_H
#define AI_PANEL_H

#include <QWidget>
#include <QMap>
#include <QString>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QShowEvent>
#include <QTimer>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QJsonObject>
#include <QProcess>

class Neuro_programm;

class AI_panel : public QWidget {
    Q_OBJECT
    friend class Neuro_programm;

public:
    explicit AI_panel(QWidget *parent = nullptr);
    ~AI_panel() override = default;

    bool buildUiFromConfig(const QString& schemaPath);
    //bool saveFieldsToYaml(const QString& projectPath, const char16_t *newParameter = u"");
    bool loadFieldsFromYaml(const QString& projectPath);

    Neuro_programm* wf = nullptr;

protected:
    void showEvent(QShowEvent *event) override;

signals:
    void pipelineActivated();

private slots:
    void onArchitectureChanged(int index);

private:
    QVector<QString> m_currentArchDescs;
    void clearLayout(QLayout* layout);
    void updateArchitectureMapping(const QString& modeId, const QJsonObject& fieldObj);
    int getRealFileCount(const QString& modeId);
    void setupAutoSaveTriggers(QWidget* widget, const QString& type);

    // Основная карта зарегистрированных виджетов PyTorch
    QMap<QString, QWidget*> m_widgetsMap;

    // Внутренние кэшированные объекты схемы для MLOps логики
    QJsonObject m_modelArchFieldObj;

    // Управляющие элементы MLOps
    QButtonGroup* m_modeGroup = nullptr;
    QComboBox* m_comboArchitecture = nullptr;
    QLabel* m_lblArchDesc = nullptr;
    QLabel* m_lblPipelineStatus = nullptr;
    QPushButton* m_btnActivate = nullptr;

    // Инфраструктурные указатели Qt
    QVBoxLayout *m_mainLayout = nullptr;
    QWidget *m_scrollContentWidget = nullptr;
    QTimer *m_saveTimer = nullptr;
    void verifyAndUnlockPipeline();
    void triggerAutoSave();
    //void onProcessTrainingFinished(int exitCode, QProcess::ExitStatus exitStatus);
    bool saveFieldsToYaml(const QString &projectPath);
};

#endif // AI_PANEL_H
