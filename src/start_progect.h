#ifndef START_PROGECT_H
#define START_PROGECT_H

#include <QDialog>

class Neuro_programm;

namespace Ui {
class Start_progect;
}

class Start_progect : public QDialog
{
    Q_OBJECT

public:
    explicit Start_progect(QWidget *parent = nullptr);
    ~Start_progect();
    Neuro_programm *wf;
    QString getDatasetLocation() const;
    bool shouldInstallVenv() const;
    QString getPyTorchArchitecture() const;
    Neuro_programm* mainWin = nullptr;

    QString getProjectName() const;
    QString getProjectLocation() const;
    bool isCreateNewVenv() const;
    QString getCreateNewVenvPath() const;
    bool isUseExistingVenv() const;
    QString getExistingVenvPath() const;
    bool isGpuArchitecture() const;
    bool isCpuArchitecture() const;
    QString getDatasetPath() const;
    bool isSymlinkMode() const;
    bool isCopyMode() const;
    bool islineEditDatasetPath() const;
    bool isDatasetEnabled() const;
    bool isCustomRequirementsEnabled() const;
    QString getCustomRequirementsPath() const;

protected slots:
    //void onNextClicked();
    void onBackClicked();
    void onexitlicked();
    void open_dyr();
    void create_progect();
    void validateFields();

private:
    Ui::Start_progect *ui;
};

#endif // START_PROGECT_H
