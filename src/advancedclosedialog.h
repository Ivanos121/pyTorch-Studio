#pragma once

#include <QDialog>
#include <QStringList>
#include <QMap>

class QCheckBox;
class QListWidget;

class AdvancedCloseDialog : public QDialog
{
    Q_OBJECT
public:
    enum ExitResult {
        ResultCancel = 0,
        ResultToTray = 1,
        ResultSaveAndExit = 2,
        ResultDiscardAndExit = 3
    };

    explicit AdvancedCloseDialog(const QStringList &modifiedFiles, bool isTraining, QWidget *parent = nullptr);

    bool shouldExportRequirements() const;
    bool shouldSaveWeights() const;

    // НОВЫЙ МЕТОД: Возвращает список только тех файлов, у которых пользователь отметил чекбокс
    QStringList getFilesToSave() const;

private:
    QCheckBox *chkExportReq{nullptr};
    QCheckBox *chkSaveWeights{nullptr};
    QListWidget *modifiedListWidget{nullptr};

    // Карта для быстрой связи чекбокса строки с абсолютным путем к файлу
    QMap<QString, QCheckBox*> m_fileCheckboxMap;
};
