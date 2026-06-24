#include "advancedclosedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>

AdvancedCloseDialog::AdvancedCloseDialog(bool hasModifiedFiles, bool isTraining, QWidget *parent)
    : QDialog(parent)
{
    // Никаких ui(new Ui::...) и ui->setupUi(this) здесь быть не должно!

    setWindowTitle("Выход из PyTorch Studio");
    setMinimumWidth(450);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel *titleLabel = new QLabel("<b>Обнаружены активные процессы и несохраненные данные:</b>", this);
    mainLayout->addWidget(titleLabel);

    if (isTraining) {
        QLabel *trainLabel = new QLabel(" ⚠️ Идет обучение модели: <i>Обработка текущих эпох...</i>", this);
        trainLabel->setStyleSheet("color: #d9534f; font-weight: bold;");
        mainLayout->addWidget(trainLabel);
    }
    if (hasModifiedFiles) {
        QLabel *filesLabel = new QLabel(" 📝 Есть несохраненные изменения в открытых файлах кода.", this);
        filesLabel->setStyleSheet("color: #f0ad4e; font-weight: bold;");
        mainLayout->addWidget(filesLabel);
    }
    if (!isTraining && !hasModifiedFiles) {
        QLabel *cleanLabel = new QLabel("Все процессы завершены, изменения сохранены.", this);
        mainLayout->addWidget(cleanLabel);
    }

    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    chkExportReq = new QCheckBox("Экспортировать окружение в requirements.txt перед выходом", this);
    chkExportReq->setChecked(true);
    mainLayout->addWidget(chkExportReq);

    chkSaveWeights = new QCheckBox("Попытаться сохранить текущие веса модели (.pth) перед остановкой", this);
    chkSaveWeights->setChecked(isTraining);
    chkSaveWeights->setEnabled(isTraining);
    mainLayout->addWidget(chkSaveWeights);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    QPushButton *btnSaveAndExit = new QPushButton(hasModifiedFiles ? "Сохранить всё и выйти" : "Выйти", this);
    btnSaveAndExit->setDefault(true);

    QPushButton *btnDiscardAndExit = new QPushButton("Выйти без сохранения", this);
    btnDiscardAndExit->setStyleSheet("QPushButton { color: #d9534f; }");

    QPushButton *btnTray = new QPushButton("Свернуть в фон", this);
    btnTray->setVisible(isTraining);

    QPushButton *btnCancel = new QPushButton("Отмена", this);

    btnLayout->addWidget(btnSaveAndExit);
    if (isTraining) {
        btnLayout->addWidget(btnTray);
    }
    btnLayout->addWidget(btnDiscardAndExit);
    btnLayout->addWidget(btnCancel);

    mainLayout->addLayout(btnLayout);

    connect(btnSaveAndExit, &QPushButton::clicked, this, [this]() { done(ResultSaveAndExit); });
    connect(btnDiscardAndExit, &QPushButton::clicked, this, [this]() { done(ResultDiscardAndExit); });
    connect(btnTray, &QPushButton::clicked, this, [this]() { done(ResultToTray); });
    connect(btnCancel, &QPushButton::clicked, this, [this]() { done(ResultCancel); });
}

bool AdvancedCloseDialog::shouldExportRequirements() const {
    return chkExportReq->isChecked();
}

bool AdvancedCloseDialog::shouldSaveWeights() const {
    return chkSaveWeights->isChecked();
}
