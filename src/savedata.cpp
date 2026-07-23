#include "savedata.h"
#include "ui_savedata.h"

Savedata::Savedata(const QStringList &modifiedFiles, const QString &projectPath, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Savedata)
    , m_proceedAllowed(false)
    , m_selectedFileToFocus("")
{
    ui->setupUi(this);

    // Стилизация и тонкая настройка под современные IDE
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(550, 240);

    ui->pushButton->setDefault(true); // Сохранить все — по кнопке ENTER
    ui->pushButton_3->setShortcut(QKeySequence(Qt::Key_Escape)); // Отмена — по ESCAPE

    // Наполняем список файлов с относительными путями в стиле JetBrains
    ui->listWidget->clear();
    for (const QString &filePath : modifiedFiles) {
        QString relativePath = QDir(projectPath).relativeFilePath(filePath);
        if (relativePath.isEmpty()) relativePath = filePath;

        QListWidgetItem *item = new QListWidgetItem(ui->listWidget);
        QFileInfo fileInfo(filePath);
        item->setText(fileInfo.fileName() + "*" + "  (" + relativePath + ")");
        item->setData(Qt::UserRole, filePath); // Прячем полный путь в память ячейки
        item->setToolTip(filePath);
    }

    // Двойной клик — запоминаем файл, на который нужно переключить вкладку
    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            m_selectedFileToFocus = item->data(Qt::UserRole).toString();
        }
        this->reject(); // Закрываем окно, отменяя дебаг/обучение
    });

    // Кнопка: Сохранить все (pushButton)
    connect(ui->pushButton, &QPushButton::clicked, this, [this]() {
        m_proceedAllowed = true;
        this->accept();
    });

    // Кнопка: Не сохранять (pushButton_2)
    connect(ui->pushButton_2, &QPushButton::clicked, this, [this]() {
        m_proceedAllowed = true;
        this->accept();
    });

    // Кнопка: Отмена (pushButton_3)
    connect(ui->pushButton_3, &QPushButton::clicked, this, &QDialog::reject);
}

Savedata::~Savedata() {
    delete ui;
}
