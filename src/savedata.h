#ifndef SAVED_DATA_H
#define SAVED_DATA_H

#include <QDialog>
#include <QFileInfo>
#include <QDir>

namespace Ui {
class Savedata; // Соответствует имени saveddata.ui из Designer [0:212]
}

class Savedata : public QDialog
{
    Q_OBJECT

public:
    explicit Savedata(const QStringList &modifiedFiles, const QString &projectPath, QWidget *parent = nullptr);
    ~Savedata();

    bool isProceedAllowed() const { return m_proceedAllowed; }
    QString getSelectedFileToFocus() const { return m_selectedFileToFocus; }

private:
    Ui::Savedata *ui;
    bool m_proceedAllowed;
    QString m_selectedFileToFocus;
};

#endif // SAVED_DATA_H
