#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
    Q_OBJECT

public:
    explicit Settings(QWidget *parent = nullptr);
    ~Settings();

protected slots:
    void close_window();
    void apply_settings();
private:
    Ui::Settings *ui;
};

#endif // SETTINGS_H
