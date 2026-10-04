#include "advancedclosedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>
#include <QFileInfo>
#include <QListWidget>
#include <QIcon>

AdvancedCloseDialog::AdvancedCloseDialog(const QStringList &modifiedFiles, bool isTraining, QWidget *parent)
    : QDialog(parent)
{
    bool hasModifiedFiles = !modifiedFiles.isEmpty();
    setWindowTitle(tr("Выход из PyTorch Studio"));
    setMinimumWidth(520); // Расширяем для удобного размещения чекбоксов и путей
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint); //

    auto *mainLayout = new QVBoxLayout(this); //
    mainLayout->setSpacing(12); //
    mainLayout->setContentsMargins(20, 20, 20, 20); //

    // =========================================================================
    // ОПРОС ДИНАМИЧЕСКИХ МЕДИА-ФЛАГОВ РОДИТЕЛЬСКОГО ОКНА
    // =========================================================================
    bool isCameraActive = false;
    bool isVideoRecording = false;

    if (parent) {
        isCameraActive = parent->property("isCameraActive").toBool();
        isVideoRecording = parent->property("isVideoRecording").toBool();
    }

    // =========================================================================
    // ПУНКТ 1: АДАПТИВНЫЙ И КОРРЕКТНЫЙ ЗАГЛОВОК ГЛАВНОГО ЛЕЙБЛА (РЕШЕНИЕ ПРОБЛЕМЫ 1)
    // =========================================================================
    QString titleText = QStringLiteral("<b>Обнаружены несохраненные данные:</b>");

    if (hasModifiedFiles && (isTraining || isCameraActive)) {
        titleText = QStringLiteral("<b>Обнаружены активные процессы или несохраненные данные:</b>");
    } else if (isTraining) {
        titleText = QStringLiteral("<b>Обнаружены активные процессы обучения:</b>");
    } else if (isCameraActive || isVideoRecording) {
        titleText = QStringLiteral("<b>Обнаружен активный видеопоток оборудования ПАК:</b>");
    }

    auto *titleLabel = new QLabel(titleText, this); //
    mainLayout->addWidget(titleLabel); //

    // КОРРЕКТИРОВКА ПОДМЕНЫ ТЕКСТА ОБУЧЕНИЯ НА ВИДЕОПОТОК
    if (isTraining && !isCameraActive) {
        auto *trainLabel = new QLabel(tr(" Идет обучение модели: <i>Обработка текущих эпох...</i>"), this); //
        trainLabel->setStyleSheet(QStringLiteral("color: #d9534f; font-weight: bold;")); //
        mainLayout->addWidget(trainLabel); //
    }
    else if (isVideoRecording) {
        auto *videoLabel = new QLabel(tr(" 🔴 Идет запись видеосессии: <i>Сохранение кадров на жесткий диск...</i>"), this);
        videoLabel->setStyleSheet(QStringLiteral("color: #e11d48; font-weight: bold;")); // Насыщенный красный
        mainLayout->addWidget(videoLabel);
    }
    else if (isCameraActive) {
        auto *cameraLabel = new QLabel(tr(" 📷 Активен ИИ-видеоконвейер: <i>Запущен захват вебкамеры / тепловизора...</i>"), this);
        cameraLabel->setStyleSheet(QStringLiteral("color: #d97706; font-weight: bold;")); // Предупреждающий оранжевый
        mainLayout->addWidget(cameraLabel);
    }

    if (hasModifiedFiles) {
        auto *filesLabel = new QLabel(tr(" Выберите измененные файлы, которые необходимо сохранить:"), this); //
        filesLabel->setStyleSheet(QStringLiteral("color: #f0ad4e; font-weight: bold;")); //
        mainLayout->addWidget(filesLabel); //

        // =====================================================================
        // СБОРКА ИНТЕГРИРОВАННОГО СПИСКА С СИСТЕМНЫМИ ЧЕКБОКСАМИ
        // =====================================================================
        modifiedListWidget = new QListWidget(this); //
        modifiedListWidget->setStyleSheet(QStringLiteral(
            "QListWidget { border: 1px solid #e4e5e6; background-color: #f8f9fa; border-radius: 4px; padding: 4px; }"
            "QListWidget::item { color: #232629; padding: 4px; border-bottom: 1px solid #f1f2f3; }"
            "QListWidget::item:last { border-bottom: none; }"
            )); //

        modifiedListWidget->setMinimumHeight(70); //
        modifiedListWidget->setMaximumHeight(140); //

        for (const QString &filePath : modifiedFiles) { //
            QFileInfo info(filePath); //
            auto *item = new QListWidgetItem(modifiedListWidget); //
            auto *rowWidget = new QWidget(modifiedListWidget); //
            auto *rowLayout = new QHBoxLayout(rowWidget); //
            rowLayout->setContentsMargins(4, 2, 4, 2); //
            rowLayout->setSpacing(8); //

            auto *fileCheck = new QCheckBox(rowWidget); //
            fileCheck->setChecked(true); //
            rowLayout->addWidget(fileCheck); //

            auto *iconLabel = new QLabel(rowWidget); //
            QIcon fileIcon = (info.suffix().toLower() == QStringLiteral("py")) //
                                 ? QIcon(QStringLiteral(":/Data/system_icons/python.svg")) //
                                 : QIcon(QStringLiteral(":/Data/system_icons/document-open.svg")); //
            iconLabel->setPixmap(fileIcon.pixmap(16, 16)); //
            rowLayout->addWidget(iconLabel); //

            auto *nameLabel = new QLabel(info.fileName(), rowWidget); //
            nameLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #232629;")); //
            rowLayout->addWidget(nameLabel); //

            auto *pathLabel = new QLabel(QStringLiteral("(%1)").arg(info.path()), rowWidget); //
            pathLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 13px; font-weight: normal;")); //
            rowWidget->setToolTip(filePath); //
            rowLayout->addWidget(pathLabel); //

            rowLayout->addStretch(1); //
            item->setSizeHint(QSize(rowWidget->sizeHint().width(), 28)); //
            modifiedListWidget->addItem(item); //
            modifiedListWidget->setItemWidget(item, rowWidget); //
            m_fileCheckboxMap.insert(filePath, fileCheck); //
        }
        mainLayout->addWidget(modifiedListWidget); //
    }

    if (!isTraining && !hasModifiedFiles && !isCameraActive) {
        auto *cleanLabel = new QLabel(tr("Все процессы завершены, изменения сохранены."), this); //
        mainLayout->addWidget(cleanLabel); //
    }

    auto *line = new QFrame(this); //
    line->setFrameShape(QFrame::HLine); //
    line->setFrameShadow(QFrame::Sunken); //
    mainLayout->addWidget(line); //

    // Чекбоксы MLOps-финализации
    chkExportReq = new QCheckBox(tr("Экспортировать окружение в requirements.txt перед выходом"), this); //
    chkExportReq->setChecked(true); //
    mainLayout->addWidget(chkExportReq); //

    chkSaveWeights = new QCheckBox(tr("Попытаться сохранить текущие веса модели (.pth) перед остановкой"), this); //
    chkSaveWeights->setChecked(isTraining); //
    chkSaveWeights->setEnabled(isTraining); //
    mainLayout->addWidget(chkSaveWeights); //
    // Ряд кнопок управления
    auto *btnLayout = new QHBoxLayout(); //
    btnLayout->setSpacing(10); //

    // ОПРЕДЕЛЯЕМ НАДПИСЬ ГЛАВНОЙ КНОПКИ ВЫХОДА
    QString exitBtnText = hasModifiedFiles ? tr("Сохранить выбранное и выйти") : tr("Выйти"); //
    if (isVideoRecording) {
        exitBtnText = tr("Прервать запись и выйти");
    }

    auto *btnSaveAndExit = new QPushButton(exitBtnText, this);
    btnSaveAndExit->setDefault(true); //

    auto *btnDiscardAndExit = new QPushButton(tr("Выйти без сохранения"), this); //
    btnDiscardAndExit->setStyleSheet(QStringLiteral("QPushButton { color: #d9534f; }")); //

    auto *btnTray = new QPushButton(tr("Свернуть в фон"), this); //

    // ИСПРАВЛЕНО: Кнопка «Свернуть в фон» должна быть активна и при обучении, и при работающей камере!
    btnTray->setVisible(isTraining || isCameraActive); //

    auto *btnCancel = new QPushButton(tr("Отмена"), this); //

    btnLayout->addWidget(btnSaveAndExit); //
    if (isTraining || isCameraActive) {
        btnLayout->addWidget(btnTray); //
    }
    btnLayout->addWidget(btnDiscardAndExit); //
    btnLayout->addWidget(btnCancel); //
    mainLayout->addLayout(btnLayout); //

    connect(btnSaveAndExit, &QPushButton::clicked, this, [this]() { done(ResultSaveAndExit); }); //
    connect(btnDiscardAndExit, &QPushButton::clicked, this, [this]() { done(ResultDiscardAndExit); }); //
    connect(btnTray, &QPushButton::clicked, this, [this]() { done(ResultToTray); }); //
    connect(btnCancel, &QPushButton::clicked, this, [this]() { done(ResultCancel); }); //

    this->adjustSize(); //
}

bool AdvancedCloseDialog::shouldExportRequirements() const {
    return chkExportReq ? chkExportReq->isChecked() : false;
}

bool AdvancedCloseDialog::shouldSaveWeights() const {
    return chkSaveWeights ? chkSaveWeights->isChecked() : false;
}

// РЕАЛИЗАЦИЯ НОВОГО МЕТОДА: Фильтруем карту по состоянию чекбокса
QStringList AdvancedCloseDialog::getFilesToSave() const {
    QStringList filesToSave;
    for (auto it = m_fileCheckboxMap.constBegin(); it != m_fileCheckboxMap.constEnd(); ++it) {
        if (it.value() && it.value()->isChecked()) {
            filesToSave.append(it.key()); // Добавляем абсолютный путь к файлу
        }
    }
    return filesToSave;
}
