#pragma once

#include <QDialog>

class QCheckBox;
class QPushButton;

class AdvancedCloseDialog : public QDialog {
    Q_OBJECT

public:
    AdvancedCloseDialog(bool hasModifiedFiles, bool isTraining, QWidget *parent = nullptr);
    ~AdvancedCloseDialog() override = default;

    enum CustomResult {
        ResultCancel = 0,
        ResultSaveAndExit = 1,
        ResultDiscardAndExit = 2,
        ResultToTray = 3
    };

    bool shouldExportRequirements() const;
    bool shouldSaveWeights() const;

private:
    QCheckBox *chkExportReq;
    QCheckBox *chkSaveWeights;
};
