#include "advancedclosedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>
#include <QFileInfo>
#include <QListWidget>

AdvancedCloseDialog::AdvancedCloseDialog(const QStringList &modifiedFiles, bool isTraining, QWidget *parent)
    : QDialog(parent)
{
    bool hasModifiedFiles = !modifiedFiles.isEmpty();

    setWindowTitle("Выход из PyTorch Studio");
    setMinimumWidth(480); // Немного расширим для удобного чтения путей файлов
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // =========================================================================
    // ПУНКТ 1: АДАПТИВНЫЙ И КОРРЕКТНЫЙ ЗАГЛОВОК ГЛАВНОГО ЛЕЙБЛА
    // =========================================================================
    QString titleText = "<b>Обнаружены несохраненные данные:</b>";
    if (hasModifiedFiles && isTraining) {
        titleText = "<b>Обнаружены активные процессы или несохраненные данные:</b>";
    } else if (isTraining) {
        titleText = "<b>Обнаружены активные процессы обучения:</b>";
    }

    QLabel *titleLabel = new QLabel(titleText, this);
    mainLayout->addWidget(titleLabel);

    if (isTraining) {
        QLabel *trainLabel = new QLabel("  Идет обучение модели: <i>Обработка текущих эпох...</i>", this);
        trainLabel->setStyleSheet("color: #d9534f; font-weight: bold;");
        mainLayout->addWidget(trainLabel);
    }

    if (hasModifiedFiles) {
        QLabel *filesLabel = new QLabel("  Есть несохраненные изменения в открытых файлах кода.", this);
        filesLabel->setStyleSheet("color: #f0ad4e; font-weight: bold;");
        mainLayout->addWidget(filesLabel);

        // =====================================================================
        // ПУНКТ 2: ИНТЕГРАЦИЯ СПИСКА ИЗМЕНЕННЫХ ФАЙЛОВ С ЖЕЛТЫМИ ИКОНКАМИ PYTHON
        // =====================================================================
        QListWidget *modifiedListWidget = new QListWidget(this);

        // Настраиваем красивый, легкий Breeze-стиль отображения списка
        modifiedListWidget->setStyleSheet(
            "QListWidget { "
            "   border: 1px solid #e4e5e6; "
            "   background-color: #f8f9fa; "
            "   border-radius: 4px; "
            "   padding: 4px; "
            "}"
            "QListWidget::item { "
            "   color: #232629; "
            "   padding: 2px; "
            "}"
            );

        // Ограничиваем высоту (65px идеально подходят для отображения 2-3 файлов)
        modifiedListWidget->setMinimumHeight(65);
        modifiedListWidget->setMaximumHeight(90);

        // Заполняем строками
        for (const QString &filePath : modifiedFiles) {
            QFileInfo info(filePath);
            QListWidgetItem *item = new QListWidgetItem(QString(" •  %1").arg(info.fileName()), modifiedListWidget);

            // Накатываем характерную желтую иконку для .py файлов
            if (info.suffix().toLower() == "py") {
                item->setIcon(QIcon(":/Data/system_icons/python.svg"));
            } else {
                item->setIcon(QIcon(":/Data/system_icons/document-open.svg"));
            }
        }
        mainLayout->addWidget(modifiedListWidget);
    }

    if (!isTraining && !hasModifiedFiles) {
        QLabel *cleanLabel = new QLabel("Все процессы завершены, изменения сохранены.", this);
        mainLayout->addWidget(cleanLabel);
    }

    // Разделительная линия
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // Чекбоксы MLOps-финализации
    chkExportReq = new QCheckBox("Экспортировать окружение в requirements.txt перед выходом", this);
    chkExportReq->setChecked(true);
    mainLayout->addWidget(chkExportReq);

    chkSaveWeights = new QCheckBox("Попытаться сохранить текущие веса модели (.pth) перед остановкой", this);
    chkSaveWeights->setChecked(isTraining);
    chkSaveWeights->setEnabled(isTraining);
    mainLayout->addWidget(chkSaveWeights);
    // Формируем ряд кнопок управления
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

    // Системные коннекты кнопок для передачи кодов в оператор switch в closeEvent
    connect(btnSaveAndExit, &QPushButton::clicked, this, [this]() { done(ResultSaveAndExit); });
    connect(btnDiscardAndExit, &QPushButton::clicked, this, [this]() { done(ResultDiscardAndExit); });
    connect(btnTray, &QPushButton::clicked, this, [this]() { done(ResultToTray); });
    connect(btnCancel, &QPushButton::clicked, this, [this]() { done(ResultCancel); });

    // Принудительно подгоняем физический размер окна ОС Linux под созданные элементы
    this->adjustSize();
}

// Методы опроса чекбоксов, которые вызываются внутри closeEvent
bool AdvancedCloseDialog::shouldExportRequirements() const {
    return chkExportReq ? chkExportReq->isChecked() : false;
}

bool AdvancedCloseDialog::shouldSaveWeights() const {
    return chkSaveWeights ? chkSaveWeights->isChecked() : false;
}
