#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QRadioButton>
#include <QMap>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>

// Предполагаем интеграцию с вашим основным классом проекта (замените на ваш базовый класс, если нужно)
class Neuro_programm;

class SpeedPanel : public QWidget {
    Q_OBJECT
public:
    explicit SpeedPanel(QWidget *parent = nullptr);
    ~SpeedPanel() = default;

    bool buildUiFromConfig(const QString& schemaPath);
    bool loadFieldsFromYaml(const QString& projectPath);
    bool saveFieldsToYaml(const QString& projectPath);

    // Связь с контекстом вашего воркфлоу (wf)
    void setWorkflowController(Neuro_programm* mainWin) { m_wf = mainWin; }

signals:
    void pipelineActivated();

private slots:
    void triggerAutoSave();
    void onArchitectureChanged(int index);
    void verifyAndUnlockPipeline();

private:
    void clearLayout(QLayout* layout);
    void setupAutoSaveTriggers(QWidget* widget, const QString& type);
    int getRealFileCount(const QString &modeId);
    void updateArchitectureMapping(const QString &modeId, const QJsonObject &fieldObj);

    // GUI Элементы управления
    QVBoxLayout*            m_mainLayout = nullptr;
    QWidget*                m_scrollContentWidget = nullptr;
    QTimer*                 m_saveTimer = nullptr;

    QMap<QString, QWidget*> m_widgetsMap;
    QStringList             m_currentArchDescs;
    QJsonObject             m_modelArchFieldObj;

    // Специфичные указатели под конвейер скорости АД
    QButtonGroup*           m_modeGroup = nullptr;
    QComboBox*              m_comboArchitecture = nullptr;
    QLabel*                 m_lblArchDesc = nullptr;
    QLabel*                 m_lblPipelineStatus = nullptr;
    QPushButton*            m_btnActivate = nullptr;

    Neuro_programm*         m_wf = nullptr;
};
