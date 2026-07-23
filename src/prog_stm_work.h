#ifndef PROG_STM_WORK_H
#define PROG_STM_WORK_H

#include <QWidget>

namespace Ui
{
class Prog_STM_work;
}

class Prog_STM_work : public QWidget
{
    Q_OBJECT

public:
    explicit Prog_STM_work(QWidget *parent = nullptr);
    ~Prog_STM_work();

private:
    Ui::Prog_STM_work *ui;
};

#endif // PROG_STM_WORK_H
