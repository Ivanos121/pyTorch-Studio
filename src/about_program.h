#ifndef ABOUT_PROGRAM_H
#define ABOUT_PROGRAM_H

#include <QDialog>
#include <QProcess>

namespace Ui {
class About_program;
}

class About_program : public QDialog
{
    Q_OBJECT

public:
    explicit About_program(QWidget *parent = nullptr);
    ~About_program();

protected slots:
    void close_window();

private slots:
    void onAiStackCheckFinished();
    void startAsyncAiStackCheck();
    void onCopyButtonClicked();

private:
    Ui::About_program *ui;
    QProcess *aiStackProcess = nullptr;
    QString gatherOllamaVersion() const;
    QString gatherAiStackInfo() const;
};

#endif // ABOUT_PROGRAM_H
