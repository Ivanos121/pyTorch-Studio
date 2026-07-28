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
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // =========================================================================
    // ПУНКТ 1: АДАПТИВНЫЙ И КОРРЕКТНЫЙ ЗАГЛОВОК ГЛАВНОГО ЛЕЙБЛА
    // =========================================================================
    QString titleText = QStringLiteral("<b>Обнаружены несохраненные данные:</b>");
    if (hasModifiedFiles && isTraining) {
        titleText = QStringLiteral("<b>Обнаружены активные процессы или несохраненные данные:</b>");
    } else if (isTraining) {
        titleText = QStringLiteral("<b>Обнаружены active процессы обучения:</b>");
    }

    auto *titleLabel = new QLabel(titleText, this);
    mainLayout->addWidget(titleLabel);

    if (isTraining) {
        auto *trainLabel = new QLabel(tr(" Идет обучение модели: <i>Обработка текущих эпох...</i>"), this);
        trainLabel->setStyleSheet(QStringLiteral("color: #d9534f; font-weight: bold;"));
        mainLayout->addWidget(trainLabel);
    }

    if (hasModifiedFiles) {
        auto *filesLabel = new QLabel(tr(" Выберите измененные файлы, которые необходимо сохранить:"), this);
        filesLabel->setStyleSheet(QStringLiteral("color: #f0ad4e; font-weight: bold;"));
        mainLayout->addWidget(filesLabel);

        // =====================================================================
        // ИСПРАВЛЕНО: СБОРКА ИНТЕГРИРОВАННОГО СПИСКА С СИСТЕМНЫМИ ЧЕКБОКСАМИ
        // =====================================================================
        modifiedListWidget = new QListWidget(this);
        modifiedListWidget->setStyleSheet(QStringLiteral(
            "QListWidget { border: 1px solid #e4e5e6; background-color: #f8f9fa; border-radius: 4px; padding: 4px; }"
            "QListWidget::item { color: #232629; padding: 4px; border-bottom: 1px solid #f1f2f3; }"
            "QListWidget::item:last { border-bottom: none; }"
            ));

        // ИСПРАВЛЕНО: Уменьшаем минимальную высоту контейнера до 70px (под 2-3 файла идеально)
        modifiedListWidget->setMinimumHeight(70);
        modifiedListWidget->setMaximumHeight(140);

        for (const QString &filePath : modifiedFiles) {
            QFileInfo info(filePath);

            // 1. Создаем базовый элемент списка
            auto *item = new QListWidgetItem(modifiedListWidget);

            // 2. ИСПРАВЛЕНО: Создаем кастомный виджет строки и ОДНОВРЕМЕННО
            // привязываем к нему горизонтальный менеджер компоновки!
            auto *rowWidget = new QWidget(modifiedListWidget);
            auto *rowLayout = new QHBoxLayout(rowWidget); // <--- Привязка rowWidget обязательна

            // Настраиваем плотные компактные отступы, чтобы строки не разъезжались
            rowLayout->setContentsMargins(4, 2, 4, 2);
            rowLayout->setSpacing(8);

            // А. Чекбокс выбора сохранения
            auto *fileCheck = new QCheckBox(rowWidget);
            fileCheck->setChecked(true);
            rowLayout->addWidget(fileCheck);

            // Б. Иконка типа файла
            auto *iconLabel = new QLabel(rowWidget);
            QIcon fileIcon = (info.suffix().toLower() == QStringLiteral("py"))
                                 ? QIcon(QStringLiteral(":/Data/system_icons/python.svg"))
                                 : QIcon(QStringLiteral(":/Data/system_icons/document-open.svg"));
            iconLabel->setPixmap(fileIcon.pixmap(16, 16));
            rowLayout->addWidget(iconLabel);

            // В. Имя файла
            auto *nameLabel = new QLabel(info.fileName(), rowWidget);
            nameLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #232629;"));
            rowLayout->addWidget(nameLabel);

            // Г. Относительный путь к папке
            auto *pathLabel = new QLabel(QStringLiteral("(%1)").arg(info.path()), rowWidget);
            pathLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 13px; font-weight: normal;"));
            rowWidget->setToolTip(filePath);
            rowLayout->addWidget(pathLabel);

            // Заполняем пустоту справа, выравнивая всё по левому краю
            rowLayout->addStretch(1);

            // 3. ИСПРАВЛЕНО: Задаем элементу точный размер на основе макета
            // Теперь Qt6 будет знать точную физическую высоту строки!
            item->setSizeHint(QSize(rowWidget->sizeHint().width(), 28)); // 28 пикселей на строку — идеал Breeze

            modifiedListWidget->addItem(item);
            modifiedListWidget->setItemWidget(item, rowWidget);

            // Сохраняем указатель на чекбокс в карту для closeEvent
            m_fileCheckboxMap.insert(filePath, fileCheck);
        }
        mainLayout->addWidget(modifiedListWidget);
    }

    if (!isTraining && !hasModifiedFiles) {
        auto *cleanLabel = new QLabel(tr("Все процессы завершены, изменения сохранены."), this);
        mainLayout->addWidget(cleanLabel);
    }

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // Чекбоксы MLOps-финализации
    chkExportReq = new QCheckBox(tr("Экспортировать окружение в requirements.txt перед выходом"), this);
    chkExportReq->setChecked(true);
    mainLayout->addWidget(chkExportReq);

    chkSaveWeights = new QCheckBox(tr("Попытаться сохранить текущие веса модели (.pth) перед остановкой"), this);
    chkSaveWeights->setChecked(isTraining);
    chkSaveWeights->setEnabled(isTraining);
    mainLayout->addWidget(chkSaveWeights);

    // Ряд кнопок управления
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    auto *btnSaveAndExit = new QPushButton(hasModifiedFiles ? tr("Сохранить выбранное и выйти") : tr("Выйти"), this);
    btnSaveAndExit->setDefault(true);

    auto *btnDiscardAndExit = new QPushButton(tr("Выйти без сохранения"), this);
    btnDiscardAndExit->setStyleSheet(QStringLiteral("QPushButton { color: #d9534f; }"));

    auto *btnTray = new QPushButton(tr("Свернуть в фон"), this);
    btnTray->setVisible(isTraining);

    auto *btnCancel = new QPushButton(tr("Отмена"), this);

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

    this->adjustSize();
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
