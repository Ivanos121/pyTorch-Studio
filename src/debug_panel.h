#ifndef DEBUG_PANEL_H
#define DEBUG_PANEL_H

#include <QWidget>

namespace Ui
{
class Debug_panel;
}

class Debug_panel : public QWidget
{
    Q_OBJECT

public:
    explicit Debug_panel(QWidget *parent = nullptr);
    ~Debug_panel();

private:
    Ui::Debug_panel *ui;
};

#endif // DEBUG_PANEL_H
