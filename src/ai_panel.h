#ifndef AI_PANEL_H
#define AI_PANEL_H

#include <QComboBox>
#include <QProgressBar>
#include <QSpinBox>
#include <QWidget>

class Neuro_programm;

namespace Ui {
class AI_panel;
}

class AI_panel : public QWidget
{
    Q_OBJECT
    friend class Neuro_programm;

public:
    explicit AI_panel(QWidget *parent = nullptr);
    ~AI_panel();
    Neuro_programm *wf;
    QComboBox *comboBatchSize = nullptr;
    QProgressBar *progressCPU=nullptr;
    QProgressBar *progressGPU=nullptr;
    QComboBox *cosmboDevice_2=nullptr;
    QSpinBox *spinBoxEpochs=nullptr;
    QDoubleSpinBox *spinBoxLR = nullptr;
    Ui::AI_panel *ui;

    //QPushButton *btnStartTraining = nullptr;

private:
    Neuro_programm *m_mainIDE;
};

#endif // AI_PANEL_H
