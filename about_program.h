#ifndef ABOUT_PROGRAM_H
#define ABOUT_PROGRAM_H

#include <QDialog>

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
private:
    Ui::About_program *ui;
};

#endif // ABOUT_PROGRAM_H
