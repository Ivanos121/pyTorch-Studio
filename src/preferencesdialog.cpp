#include "preferencesdialog.h"
#include "configmanager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QDialogButtonBox>
#include <QPushButton>

PreferencesDialog::PreferencesDialog(QWidget *parent) : QDialog(parent) {
    setupUi();
    loadSchemaAndBuildUi();

    // МАГИЯ СВЯЗЫВАНИЯ: Выбор строки в списке переключает страницу в стопке
    connect(m_listWidget, &QListWidget::currentRowChanged,
            m_stackedWidget, &QStackedWidget::setCurrentIndex);

    // По умолчанию открываем первую страницу
    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }
}

void PreferencesDialog::setupUi() {
    this->setWindowTitle(tr("Настройки PyTorch Studio"));
    this->resize(800, 540); // Немного увеличиваем высоту окна, чтобы кнопки помещались без сжатия контента

    // 1. Главный КОРНЕВОЙ вертикальный слой для всего окна
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    // 2. Горизонтальный слой для контента (список категорий + стопка страниц настроек)
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(15);

    // Левое меню категорий
    m_listWidget = new QListWidget(this);
    m_listWidget->setFixedWidth(210);
    m_listWidget->setSpacing(3);

    // Правая стопка страниц
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setFrameShape(QFrame::StyledPanel);

    // Собираем контентную часть вместе
    contentLayout->addWidget(m_listWidget);
    contentLayout->addWidget(m_stackedWidget, 1);

    // Добавляем контентную часть в корневой слой с максимальным приоритетом растяжения (1)
    rootLayout->addLayout(contentLayout, 1);

    // 3. ДОБАВЛЯЕМ КНОПКИ (Решение Проблемы 1)
    // Используем QDialogButtonBox для автоматического нативного выравнивания кнопок под ОС
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        Qt::Horizontal, this
        );

    // Добавляем блок кнопок в самый низ корневого слоя
    rootLayout->addWidget(buttonBox);

    // 4. Логика работы кнопок
    // Кнопка OK — сохраняет/закрывает окно (код возврата QDialog::Accepted)
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    // Кнопка Cancel — просто закрывает окно без фиксации изменений (код возврата QDialog::Rejected)
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Кнопка Apply (Применить) — активирует сохранение данных без закрытия диалога
    QPushButton *applyButton = buttonBox->button(QDialogButtonBox::Apply);
    if (applyButton) {
        connect(applyButton, &QPushButton::clicked, this, []() {
            // Наша Data-Driven архитектура сохраняет параметры по цепочке сигналов сразу,
            // но при желании здесь можно принудительно обновить системный кэш:
            // ConfigManager::instance().sync();
        });
    }
}

void PreferencesDialog::loadSchemaAndBuildUi() {
    // Читаем файл схемы из ресурсов Qt (файл должен быть добавлен в ваш .qrc)
    QFile file(":/Config/settings_schema.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Критическая ошибка: файл settings_schema.json не найден в ресурсах!";
        return;
    }

    QJsonArray schemaArray = QJsonDocument::fromJson(file.readAll()).array();
    file.close();

    // Обходим массив настроек из JSON
    for (const QJsonValue &val : std::as_const(schemaArray))
    {
        QJsonObject setting = val.toObject();
        QString category = setting["category"].toString();
        QString labelText = setting["label"].toString();
        QString description = setting["description"].toString();

        // 1. Если такой категории страниц еще нет — создаем ее динамически
        if (!m_categoryLayouts.contains(category)) {
            QWidget *pageWidget = new QWidget();
            QVBoxLayout *pageLayout = new QVBoxLayout(pageWidget);
            pageLayout->setContentsMargins(15, 15, 15, 15);

            QLabel *titleLabel = new QLabel(QString("<h2>%1</h2>").arg(category), pageWidget);
            pageLayout->addWidget(titleLabel);

            QScrollArea *scrollArea = new QScrollArea(pageWidget);
            scrollArea->setWidgetResizable(true);
            scrollArea->setFrameShape(QFrame::NoFrame);
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

            QWidget *scrollContent = new QWidget(scrollArea);
            QFormLayout *formLayout = new QFormLayout(scrollContent);

            // Настройка режима распределения колонок против горизонтального сдвига
            formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
            formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            formLayout->setContentsMargins(10, 10, 25, 10);
            formLayout->setSpacing(15);

            scrollContent->setLayout(formLayout);
            scrollArea->setWidget(scrollContent);
            pageLayout->addWidget(scrollArea, 1);

            m_listWidget->addItem(category);
            m_stackedWidget->addWidget(pageWidget);
            m_categoryLayouts[category] = formLayout;
        }

        // 2. Генерируем виджет через нашу внутреннюю фабрику
        QWidget *controlWidget = createWidgetForType(setting);

        if (controlWidget) {
            // Создаем блок: Название + Виджет + Описание снизу серым шрифтом
            QVBoxLayout *itemBlockLayout = new QVBoxLayout();
            itemBlockLayout->setSpacing(2);
            itemBlockLayout->setContentsMargins(0, 0, 0, 0);
            itemBlockLayout->addWidget(controlWidget);

            if (!description.isEmpty()) {
                QLabel *descLabel = new QLabel(description);
                descLabel->setStyleSheet("color: gray; font-size: 11px;");
                descLabel->setWordWrap(true); // Гарантирует автоперенос строк
                descLabel->setMaximumWidth(420); // Ограничиваем ширину подсказки вширь
                itemBlockLayout->addWidget(descLabel);
            }

            // Создаем жестко фиксированную левую текстовую метку
            QLabel *rowLabel = new QLabel(labelText + ":");
            rowLabel->setWordWrap(true);    // Разрешаем перенос длинных названий ИИ-параметров
            rowLabel->setFixedWidth(200);   // Левая колонка строго заперта в лимит 200px

            // Добавляем готовую строку в форму текущей категории
            m_categoryLayouts[category]->addRow(rowLabel, itemBlockLayout);
        }
    }
}

// УНИВЕРСАЛЬНАЯ ФАБРИКА КОМПОНЕНТОВ СВЯЗИ
QWidget* PreferencesDialog::createWidgetForType(const QJsonObject &settingObj)
{
    QString type = settingObj["type"].toString();
    QString key = settingObj["key"].toString();
    QVariant defaultValue = settingObj["default"].toVariant();

    // Запрашиваем текущее значение бэкенда (или дефолтное, если на диске еще пусто)
    QVariant currentVal = ConfigManager::instance().getValue(key, defaultValue);

    if (type == "bool") {
        QCheckBox *checkBox = new QCheckBox();
        checkBox->setChecked(currentVal.toBool());

        connect(checkBox, &QCheckBox::toggled, [key](bool checked) {
            ConfigManager::instance().setValue(key, checked);
        });
        return checkBox;
    }
    else if (type == "int") {
        QSpinBox *spinBox = new QSpinBox();
        spinBox->setRange(settingObj["min"].toInt(0), settingObj["max"].toInt(100));
        spinBox->setValue(currentVal.toInt());

        // ИСПРАВЛЕНИЕ: Ограничиваем ширину числового поля, чтобы оно не растягивалось
        spinBox->setMaximumWidth(120);

        connect(spinBox, &QSpinBox::valueChanged, [key](int val) {
            ConfigManager::instance().setValue(key, val);
        });
        return spinBox;
    }
    else if (type == "enum")
    {
        QComboBox *comboBox = new QComboBox();
        QJsonArray options = settingObj["options"].toArray();
        for (const QJsonValue &opt : std::as_const(options))
        {
            comboBox->addItem(opt.toString());
        }
        comboBox->setCurrentIndex(currentVal.toInt());

        // ИСПРАВЛЕНИЕ: Выпадающие списки больше не будут шире 350 пикселей
        comboBox->setMaximumWidth(350);

        connect(comboBox, &QComboBox::currentIndexChanged, [key](int index) {
            ConfigManager::instance().setValue(key, index);
        });
        return comboBox;
    }
    else if (type == "string") {
        QLineEdit *lineEdit = new QLineEdit();
        lineEdit->setText(currentVal.toString());

        // ИСПРАВЛЕНИЕ: Строки ввода путей и URL теперь имеют фиксированный аккуратный размер
        lineEdit->setMaximumWidth(350);

        connect(lineEdit, &QLineEdit::textChanged, [key](const QString &text) {
            ConfigManager::instance().setValue(key, text);
        });
        return lineEdit;
    }

    return nullptr;
}

