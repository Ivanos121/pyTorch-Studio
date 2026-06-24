#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include <QListWidgetItem>
#include "codeeditor.h"


class PythonHighlighter;

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
    Q_OBJECT

public:
    explicit Settings(QWidget *parent = nullptr);
    ~Settings();

signals:
    void settingsChanged();

private slots:
    void onCategoryChanged(QListWidgetItem *current, QListWidgetItem *previous);
    //void onSaveClicked();
    void updatePreviewTheme();
    void btnOk_clicked();
    void btnApply_clicked();
    void btnClose_clicked();
    //void close_window();
    //void apply_settings();

private:
    Ui::Settings *ui;
    void loadCurrentSettings();
    PythonHighlighter *previewHighlighter = nullptr;
    void initThemesComboBox();
};

#endif // SETTINGS_H
