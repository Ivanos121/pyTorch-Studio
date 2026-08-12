#pragma once
#include <QWidget>
#include <QRadioButton>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QJsonObject>
#include <QJsonArray>
#include <qpushbutton.h>

class TrainConfigWizard : public QWidget {
    Q_OBJECT
public:
    explicit TrainConfigWizard(QWidget *parent = nullptr);
    void loadConfigAndBuildUI(const QString &configPath);

signals:
    void settingsValidated(); // Сигнал для активации кнопки "Обучение" на боковой панели

private slots:
    void onModeToggled(int id);
    void saveCurrentSettings();

private:
    QString m_configPath;
    QJsonObject m_rootConfig;

    // Динамические элементы UI
    QVBoxLayout *m_mainLayout;
    QButtonGroup *m_modeGroup;
    QComboBox *m_comboArch;
    QLabel *m_lblDesc;
    QLabel *m_lblStatus;
    QPushButton *m_btnApply;

    void updateArchComboBox(const QString &modeId);
    int getRealFileCount(const QString &modeId);
};
