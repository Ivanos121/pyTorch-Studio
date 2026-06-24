#ifndef HELP_WINDOW_H
#define HELP_WINDOW_H

#include <QWidget>

namespace Ui {
class Help_window;
}

class Help_window : public QWidget
{
    Q_OBJECT

public:
    explicit Help_window(QWidget *parent = nullptr);
    ~Help_window();

private:
    Ui::Help_window *ui;
};

#endif // HELP_WINDOW_H
