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
    QString getProjectName() const;
    QString getProjectLocation() const;
    QString getDatasetLocation() const;
    bool shouldInstallVenv() const;
    QString getPyTorchArchitecture() const;

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
