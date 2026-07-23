#ifndef PROG_STM_H
#define PROG_STM_H

#include <QWidget>


namespace Ui
{
class Prog_STM;
}

class Neuro_programm;

class Prog_STM : public QWidget
{
    Q_OBJECT

public:
    explicit Prog_STM(QWidget *parent = nullptr);
    ~Prog_STM();

    void setMainProgram(Neuro_programm *mainProgram);
    // Метод для обновления текстовой строки статуса на экране
    void updateStatusText(const QString &text, const QString &colorHtml);

    // Метод для изменения доступности экранных кнопок прошивки и стирания
    void setFlashButtonsEnabled(bool enabled);
    void setDeviceHardwareInfo(const QString &chipModel, int flashKb, int sramKb, bool isConnected);


private:
    Ui::Prog_STM *ui;
    Neuro_programm *m_mainProgram;
};

#endif // PROG_STM_H
