#include "trainconfigwizard.h"
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QMessageBox>

TrainConfigWizard::TrainConfigWizard(QWidget *parent) : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_modeGroup = new QButtonGroup(this);

    m_comboArch = new QComboBox(this);
    m_lblDesc = new QLabel("Описание архитектуры...", this);
    m_lblStatus = new QLabel(this);
    m_btnApply = new QPushButton("Применить настройки", this);

    m_lblDesc->setWordWrap(true);
    m_lblDesc->setStyleSheet("color: gray; font-style: italic;");

    connect(m_modeGroup, &QButtonGroup::idClicked, this, &TrainConfigWizard::onModeToggled);
    connect(m_btnApply, &QPushButton::clicked, this, &TrainConfigWizard::saveCurrentSettings);
}

void TrainConfigWizard::loadConfigAndBuildUI(const QString &configPath) {
    m_configPath = configPath;

    // 1. Читаем локальный JSON конфиг
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    m_rootConfig = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    // Очищаем старые радиокнопки, если они были
    for (QAbstractButton *btn : m_modeGroup->buttons()) {
        m_mainLayout->removeWidget(btn);
        delete btn;
    }

    // 2. Парсим схему из конфига и строим Радиокнопки режимов
    QJsonArray modesArray = m_rootConfig["schema"].toObject()["modes"].toArray();

    m_mainLayout->addWidget(new QLabel("<b>1. Выберите режим инференса:</b>", this));

    for (int i = 0; i < modesArray.size(); ++i) {
        QJsonObject modeObj = modesArray[i].toObject();
        QString id = modeObj["id"].toString();
        QString label = modeObj["label"].toString();
        int minRequired = modeObj["min_files_required"].toInt();

        QRadioButton *radio = new QRadioButton(label, this);
        m_modeGroup->addButton(radio, i);
        m_mainLayout->addWidget(radio);

        // ВАЛИДАЦИЯ ДАННЫХ: Проверяем реальное количество файлов на диске
        int realFiles = getRealFileCount(id);
        if (realFiles < minRequired) {
            radio->setEnabled(false);
            radio->setText(label + QString(" (Недоступно: найдено %1 файлов, нужно %2)").arg(realFiles).arg(minRequired));
        }
    }

    // 3. Добавляем в разметку комбобокс архитектур и описание
    m_mainLayout->addWidget(new QLabel("<br><b>2. Выберите архитектуру нейросети:</b>", this));
    m_mainLayout->addWidget(m_comboArch);
    m_mainLayout->addWidget(m_lblDesc);
    m_mainLayout->addSpacing(20);
    m_mainLayout->addWidget(m_lblStatus);
    m_mainLayout->addWidget(m_btnApply);
    m_mainLayout->addStretch();

    // Подгружаем сохраненный ранее режим
    QString savedMode = m_rootConfig["current_settings"].toObject()["selected_mode"].toString();
    for (int i = 0; i < modesArray.size(); ++i) {
        if (modesArray[i].toObject()["id"].toString() == savedMode && m_modeGroup->button(i)->isEnabled()) {
            m_modeGroup->button(i)->setChecked(true);
            updateArchComboBox(savedMode);
            break;
        }
    }
}

// Эмуляция/проверка подсчета файлов в универсальной структуре
int TrainConfigWizard::getRealFileCount(const QString &modeId) {
    if (modeId == "sensors") {
        return QDir("./datasets/sensors_csv").entryList(QDir::Files).count();
    } else if (modeId == "thermal" || modeId == "hybrid") {
        int normal = QDir("./data/raw/normal").entryList(QDir::Files).count();
        int overheat = QDir("./data/raw/overheat").entryList(QDir::Files).count();
        return normal + overheat;
    }
    return 0;
}

// Слот переключения радиокнопок: динамически меняет список нейросетей
void TrainConfigWizard::onModeToggled(int id) {
    QJsonArray modesArray = m_rootConfig["schema"].toObject()["modes"].toArray();
    QString modeId = modesArray[id].toObject()["id"].toString();
    updateArchComboBox(modeId);
}

void TrainConfigWizard::updateArchComboBox(const QString &modeId) {
    m_comboArch->clear();
    QJsonArray modesArray = m_rootConfig["schema"].toObject()["modes"].toArray();

    for (QJsonValue modeVal : modesArray) {
        QJsonObject modeObj = modeVal.toObject();
        if (modeObj["id"].toString() == modeId) {
            QJsonArray archs = modeObj["architectures"].toArray();
            for (QJsonValue archVal : archs) {
                QJsonObject archObj = archVal.toObject();
                // Сохраняем ID сети в Data-слот, а текст выводим пользователю
                m_comboArch->addItem(archObj["label"].toString(), archObj["id"].toString());
            }
            break;
        }
    }

    // Обновляем описание при смене элемента в комбобоксе
    connect(m_comboArch, &QComboBox::currentIndexChanged, [this, modeId](int index) {
        if (index < 0) return;
        QString archId = m_comboArch->currentData().toString();
        QJsonArray modes = m_rootConfig["schema"].toObject()["modes"].toArray();
        for (QJsonValue m : modes) {
            if (m.toObject()["id"].toString() == modeId) {
                for (QJsonValue a : m.toObject()["architectures"].toArray()) {
                    if (a.toObject()["id"].toString() == archId) {
                        m_lblDesc->setText(a.toObject()["desc"].toString());
                        return;
                    }
                }
            }
        }
    });
    if (m_comboArch->count() > 0) m_comboArch->setCurrentIndex(0);
}

// 4. ЗАПИСЬ НАСТРОЕК В ЛОКАЛЬНЫЙ КОНФИГ ПРИ НАЖАТИИ "ПРИМЕНИТЬ"
void TrainConfigWizard::saveCurrentSettings() {
    int checkedId = m_modeGroup->checkedId();
    if (checkedId == -1) return;

    QJsonArray modesArray = m_rootConfig["schema"].toObject()["modes"].toArray();
    QString modeId = modesArray[checkedId].toObject()["id"].toString();
    QString archId = m_comboArch->currentData().toString();

    // Обновляем секцию текущих настроек в JSON структуре
    QJsonObject currentSettings;
    currentSettings["selected_mode"] = modeId;
    currentSettings["selected_architecture"] = archId;
    m_rootConfig["current_settings"] = currentSettings;

    // Перезаписываем файл на диске
    QFile file(m_configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument doc(m_rootConfig);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        m_lblStatus->setText("<font color='#2ecc71'><b>✓ Настройки сохранены в конфиг! Кнопка 'Обучение' активна.</b></font>");
        emit settingsValidated(); // Активируем кнопку старта на боковой панели panelOther
    }
}
