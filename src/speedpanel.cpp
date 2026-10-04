#include "speedpanel.h"
#include "neuro_programm.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QScrollArea>
#include <QShowEvent>
#include <QCoreApplication>
#include <QFrame>
#include <QSettings>
#include <QDebug>

SpeedPanel::SpeedPanel(QWidget *parent) : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(15);

    // Таймер умного автосохранения манифеста hyperparameters.yaml
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    connect(m_saveTimer, &QTimer::timeout, this, [this]() {
        QString currentPath = QStringLiteral("/home/elf/zcc/z1"); // Базовый эталонный путь к проекту
        this->saveFieldsToYaml(currentPath);

        if (m_btnActivate && m_lblPipelineStatus) {
            m_lblPipelineStatus->setText(QStringLiteral(" Настройки автосохранены. Требуется повторная верификация наблюдателя скорости."));
            m_btnActivate->setEnabled(true);
            m_btnActivate->setText(QStringLiteral(" АКТИВИРОВАТЬ КОНВЕЙЕР ОБУЧЕНИЯ (НАБЛЮДАТЕЛЬ СКОРОСТИ)"));
            m_btnActivate->setStyleSheet(QStringLiteral("background-color: #2980b9; color: white; font-weight: bold; height: 35px; border-radius: 4px;"));
        }
    });
}

void SpeedPanel::clearLayout(QLayout* layout) {
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        } else if (QLayout* childLayout = item->layout()) {
            clearLayout(childLayout);
        }
        delete item;
    }
}

bool SpeedPanel::buildUiFromConfig(const QString& schemaPath) {
    if (!m_mainLayout) return false;
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    clearLayout(m_mainLayout);
    m_widgetsMap.clear();

    QFile file(schemaPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << " [SpeedUI]: Файл схемы конфигурации не найден:" << schemaPath;
        return false;
    }
    QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    file.close();

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_scrollContentWidget = new QWidget(scrollArea);
    QVBoxLayout *containerLayout = new QVBoxLayout(m_scrollContentWidget);
    containerLayout->setContentsMargins(10, 10, 10, 10);
    containerLayout->setSpacing(15);

    QWidget *topFloorWidget = new QWidget(m_scrollContentWidget);
    QGridLayout *gridLayout = new QGridLayout(topFloorWidget);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(15);
    containerLayout->addWidget(topFloorWidget);

    QMap<QString, QFormLayout*> groupsMap;

    QGroupBox* hyperBox = new QGroupBox(QStringLiteral("Блок настроек гиперпараметров"), m_scrollContentWidget);
    hyperBox->setStyleSheet(QStringLiteral("QGroupBox { font-weight: bold; }"));
    QFormLayout* hyperForm = new QFormLayout(hyperBox);
    hyperForm->setLabelAlignment(Qt::AlignLeft);
    hyperForm->setContentsMargins(12, 14, 12, 12);
    hyperForm->setSpacing(10);
    groupsMap[QStringLiteral("hyper")] = hyperForm;
    gridLayout->addWidget(hyperBox, 0, 0);

    QGroupBox* hardwareBox = new QGroupBox(QStringLiteral("Аппаратная конфигурация и логирование"), m_scrollContentWidget);
    hardwareBox->setStyleSheet(QStringLiteral("QGroupBox { font-weight: bold; }"));
    QFormLayout* hardwareForm = new QFormLayout(hardwareBox);
    hardwareForm->setLabelAlignment(Qt::AlignLeft);
    hardwareForm->setContentsMargins(12, 14, 12, 12);
    hardwareForm->setSpacing(10);
    groupsMap[QStringLiteral("hardware")] = hardwareForm;
    gridLayout->addWidget(hardwareBox, 0, 1);

    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 1);

    QGroupBox* mlopsBox = new QGroupBox(QStringLiteral("Панель настройки конвейера MLOps"), m_scrollContentWidget);
    mlopsBox->setStyleSheet(QStringLiteral("QGroupBox { font-weight: bold; }"));
    QVBoxLayout* mlopsLayout = new QVBoxLayout(mlopsBox);
    mlopsLayout->setContentsMargins(12, 14, 12, 12);
    mlopsLayout->setSpacing(12);
    containerLayout->addWidget(mlopsBox);
    containerLayout->addStretch(1);

    for (int i = 0; i < array.size(); ++i) {
        QJsonObject param = array[i].toObject();
        if (param.contains(QStringLiteral("meta_config"))) continue;

        QString groupID = param[QStringLiteral("group")].toString();
        QString name = param[QStringLiteral("name")].toString();
        QString labelText = param[QStringLiteral("label")].toString();
        QString type = param[QStringLiteral("type")].toString();

        if (groupID == QStringLiteral("hyper") || groupID == QStringLiteral("hardware")) {
            QFormLayout* currentLayout = groupsMap[groupID];
            if (type == "int") {
                QSpinBox* spinBox = new QSpinBox(m_scrollContentWidget);
                spinBox->setRange(param["min"].toInt(0), param["max"].toInt(1000));
                spinBox->setValue(param["default"].toInt(10));
                currentLayout->addRow(labelText, spinBox);
                m_widgetsMap[name] = spinBox;
                setupAutoSaveTriggers(spinBox, type);
            }
            else if (type == "double") {
                QDoubleSpinBox* dSpinBox = new QDoubleSpinBox(m_scrollContentWidget);
                dSpinBox->setRange(param["min"].toDouble(0.0), param["max"].toDouble(1.0));
                dSpinBox->setValue(param["default"].toDouble(0.001));
                dSpinBox->setDecimals(param["decimals"].toInt(5));
                currentLayout->addRow(labelText, dSpinBox);
                m_widgetsMap[name] = dSpinBox;
                setupAutoSaveTriggers(dSpinBox, type);
            }
            else if (type == "enum") {
                QComboBox* comboBox = new QComboBox(m_scrollContentWidget);
                QJsonArray options = param["options"].toArray();
                for (int j = 0; j < options.size(); ++j) comboBox->addItem(options[j].toString());
                comboBox->setCurrentText(param["default"].toString());
                currentLayout->addRow(labelText, comboBox);
                m_widgetsMap[name] = comboBox;
                setupAutoSaveTriggers(comboBox, type);
            }
            else if (type == "bool") {
                QCheckBox* checkBox = new QCheckBox(m_scrollContentWidget);
                checkBox->setChecked(param["default"].toBool(false));
                currentLayout->addRow(labelText, checkBox);
                m_widgetsMap[name] = checkBox;
                setupAutoSaveTriggers(checkBox, type);
            }
            else if (type == "text") {
                QLineEdit* lineEdit = new QLineEdit(m_scrollContentWidget);
                lineEdit->setText(param["default"].toString());
                currentLayout->addRow(labelText, lineEdit);
                m_widgetsMap[name] = lineEdit;
                setupAutoSaveTriggers(lineEdit, type);
            }
        }
        else if (groupID == QStringLiteral("mlops")) {
            if (type == "radio_group") {
                // 1. Извлечение жирного заголовка Шага №1
                mlopsLayout->addWidget(new QLabel(QString("<b>%1</b>").arg(labelText), m_scrollContentWidget));

                // 2. СКАНИРОВАНИЕ ЛОГОВ (АККУРАТНЫЙ ЧИСТЫЙ ТЕКСТ С ОТСТУПОМ 15 ПИКСЕЛЕЙ)
                int speedFiles = getRealFileCount(QStringLiteral("speed"));

                QLabel *lblSens = new QLabel(QString(" Электрические сигналы привода (simulink_speed_dataset.csv): Найдено %1 логов").arg(speedFiles), m_scrollContentWidget);
                lblSens->setStyleSheet(speedFiles > 0
                                           ? QStringLiteral("color: #15803d; font-weight: normal; padding-left: 15px; margin-top: 4px; margin-bottom: 4px;")
                                           : QStringLiteral("color: #b91c1c; padding-left: 15px; margin-top: 4px; margin-bottom: 4px;"));
                mlopsLayout->addWidget(lblSens);

                // 3. Создание группы переключателей радиокнопок
                m_modeGroup = new QButtonGroup(m_scrollContentWidget);
                QJsonArray options = param["options"].toArray();
                for (int j = 0; j < options.size(); ++j) {
                    QJsonObject optObj = options[j].toObject();
                    QRadioButton *radio = new QRadioButton(optObj["label"].toString(), m_scrollContentWidget);
                    radio->setProperty("mode_id", optObj["id"].toString());
                    radio->setStyleSheet(QStringLiteral("QRadioButton { padding-left: 10px; }"));
                    m_modeGroup->addButton(radio, j);
                    mlopsLayout->addWidget(radio);
                }
                m_modeGroup->button(0)->setChecked(true);
                this->updateArchitectureMapping(QStringLiteral("speed_sensors"), m_modelArchFieldObj);

                // Лямбда для динамического обновления списка доступных ИИ-архитектур
                connect(m_modeGroup, &QButtonGroup::idClicked, this, [this]() {
                    if (!m_comboArchitecture || !m_modeGroup->checkedButton()) return;
                    QString modeId = m_modeGroup->checkedButton()->property("mode_id").toString();
                    this->updateArchitectureMapping(modeId, m_modelArchFieldObj);
                    this->triggerAutoSave();
                });
            }
            else if (type == "dynamic_enum") {
                m_modelArchFieldObj = param;
                mlopsLayout->addWidget(new QLabel(QString("<b>%1</b>").arg(labelText), m_scrollContentWidget));

                m_comboArchitecture = new QComboBox(m_scrollContentWidget);
                m_comboArchitecture->setObjectName(name);

                m_lblArchDesc = new QLabel(m_scrollContentWidget);
                m_lblArchDesc->setStyleSheet(QStringLiteral("color: #7f8c8d; font-style: italic; padding-left: 10px;"));

                mlopsLayout->addWidget(m_comboArchitecture);
                mlopsLayout->addWidget(m_lblArchDesc);

                // =========================================================================
                // ЖЕСТКИЙ ФИКС: НАПОЛНЯЕМ И АКТИВИРУЕМ КОМБОБОКС ПО УМОЛЧАНИЮ ПРИ СТАРТЕ
                // =========================================================================
                // 1. Принудительно вызываем маппинг для дефолтного режима 'speed_sensors'
                this->updateArchitectureMapping(QStringLiteral("speed_sensors"), m_modelArchFieldObj);

                // 2. Силово снимаем блокировку интерфейса Qt, чтобы комбобокс стал кликабельным
                m_comboArchitecture->blockSignals(false);
                m_comboArchitecture->setEnabled(true);

                connect(m_comboArchitecture, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpeedPanel::triggerAutoSave);
            }
            else if (type == "enum") {
                mlopsLayout->addWidget(new QLabel(QString("<b>%1</b>").arg(labelText), m_scrollContentWidget));
                QComboBox* cbVis = new QComboBox(m_scrollContentWidget);
                QJsonArray options = param["options"].toArray();
                for (int j = 0; j < options.size(); ++j) cbVis->addItem(options[j].toString());
                cbVis->setCurrentText(param["default"].toString());
                mlopsLayout->addWidget(cbVis);
                m_widgetsMap[name] = cbVis;
                connect(cbVis, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpeedPanel::triggerAutoSave);
            }
            else if (type == "label") {
                QLabel* statusLabel = new QLabel(labelText, m_scrollContentWidget);
                statusLabel->setStyleSheet(QStringLiteral("color: #38bdf8; font-family: monospace; font-weight: bold; padding-left: 10px;"));
                mlopsLayout->addWidget(statusLabel);
            }
            else if (type == "action_button") {
                mlopsLayout->addSpacing(10);
                m_lblPipelineStatus = new QLabel(QStringLiteral(" Настройки автосохранены. Требуется повторная верификация наблюдателя скорости."), m_scrollContentWidget);
                m_lblPipelineStatus->setStyleSheet(QStringLiteral("QLabel { padding-left: 5px; }"));
                mlopsLayout->addWidget(m_lblPipelineStatus);

                m_btnActivate = new QPushButton(labelText, m_scrollContentWidget);
                m_btnActivate->setStyleSheet(QStringLiteral("background-color: #2980b9; color: white; font-weight: bold; height: 35px; border-radius: 4px;"));
                connect(m_btnActivate, &QPushButton::clicked, this, &SpeedPanel::verifyAndUnlockPipeline);
                mlopsLayout->addWidget(m_btnActivate);
            }
        }
    }
    // ... (весь ваш существующий цикл генерации элементов scrollArea) ...
    scrollArea->setWidget(m_scrollContentWidget);
    m_mainLayout->addWidget(scrollArea);

    // 1. Сначала даём Студии прочитать сохранённый hyperparameters.yaml
    QString currentProject = m_wf ? m_wf->getCurrentProjectPath() : QString();
    if (currentProject.isEmpty()) currentProject = QStringLiteral("/home/elf/pyTorch-Studio");
    this->loadFieldsFromYaml(currentProject);

    // =========================================================================
    // ЖЕСТКИЙ ФИКС: РАЗМОРАЖИВАЕМ И НАПОЛНЯЕМ КОМБОБОКС АРХИТЕКТУР ПРИ СТАРТЕ ПАК
    // =========================================================================
    if (m_comboArchitecture) {
        // Проверяем, какая радиокнопка сейчас реально активна на экране
        QString activeModeId = QStringLiteral("speed_sensors");
        if (m_modeGroup && m_modeGroup->checkedButton()) {
            activeModeId = m_modeGroup->checkedButton()->property("mode_id").toString();
        }

        // Силово перестраиваем и наполняем комбобокс записями из схемы
        this->updateArchitectureMapping(activeModeId, m_modelArchFieldObj);

        // Намертво снимаем блокировку Qt и делаем элемент активным для мыши
        m_comboArchitecture->blockSignals(false);
        m_comboArchitecture->setEnabled(true);
        m_comboArchitecture->setFocusPolicy(Qt::StrongFocus);
    }

    this->update();
    return true;
}

void SpeedPanel::setupAutoSaveTriggers(QWidget* widget, const QString& type) {
    if (type == "int") connect(qobject_cast<QSpinBox*>(widget), &QSpinBox::valueChanged, this, &SpeedPanel::triggerAutoSave);
    else if (type == "double") connect(qobject_cast<QDoubleSpinBox*>(widget), &QDoubleSpinBox::valueChanged, this, &SpeedPanel::triggerAutoSave);
    else if (type == "enum") connect(qobject_cast<QComboBox*>(widget), &QComboBox::currentIndexChanged, this, &SpeedPanel::triggerAutoSave);
    else if (type == "bool") connect(qobject_cast<QCheckBox*>(widget), &QCheckBox::toggled, this, &SpeedPanel::triggerAutoSave);
    else if (type == "text") connect(qobject_cast<QLineEdit*>(widget), &QLineEdit::textChanged, this, &SpeedPanel::triggerAutoSave);
}

void SpeedPanel::triggerAutoSave() {
    if (m_saveTimer) m_saveTimer->start(3000);
}

int SpeedPanel::getRealFileCount(const QString &) {
    // Сканируем выделенную папку строго на наличие датасетов скорости
    QString speedDatasetPath = QStringLiteral("/home/elf/zcc/z1/datasets/speed_csv");
    QStringList filters; filters << QStringLiteral("simulink_speed_dataset.csv");
    return QDir(speedDatasetPath).entryList(filters, QDir::Files).count();
}

void SpeedPanel::updateArchitectureMapping(const QString &modeId, const QJsonObject &fieldObj) {
    if (!m_comboArchitecture) return;

    // 1. ПОЛНАЯ ОЧИСТКА В РЕЖИМЕ ЗАМОРОЗКИ
    m_comboArchitecture->blockSignals(true);
    m_comboArchitecture->clear();
    m_currentArchDescs.clear();

    // 2. ЦИКЛ НАПОЛНЕНИЯ ЗАПИСЯМИ ИЗ JSON-СХЕМЫ ПАК
    QJsonArray arr = fieldObj["mapping"].toObject().value(modeId).toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject archObj = arr[i].toObject();
        m_comboArchitecture->addItem(archObj["label"].toString(), archObj["id"].toString());
        m_currentArchDescs.append(archObj["desc"].toString());
    }

    // =========================================================================
    // СВЕРХВАЖНЫЙ ПОРЯДОК: СНАЧАЛА РАЗМОРАЖИВАЕМ СИГНАЛЫ, ПОТОМ ВЫБИРАЕМ ИНДЕКС
    // =========================================================================
    m_comboArchitecture->blockSignals(false); // <--- СНАЧАЛА СНИМАЕМ БЛОКИРОВКУ QT!

    if (m_comboArchitecture->count() > 0) {
        // Силово выставляем индекс 0. Теперь Qt увидит событие и прорисует текст!
        m_comboArchitecture->setCurrentIndex(0);

        // Настраиваем жесткую светлую палитру, чтобы текст не перекрашивался системой
        m_comboArchitecture->setStyleSheet(QStringLiteral(
            "QComboBox { color: #1e1e1e !important; background-color: #ffffff !important; border: 1px solid #cccccc; padding: 3px 5px; min-height: 22px; }"
            "QComboBox QAbstractItemView { color: #1e1e1e !important; background-color: #ffffff !important; }"
            ));

        // Если это редактируемый комбобокс (Editable), дублируем команду заполнения
        if (m_comboArchitecture->isEditable() && m_comboArchitecture->lineEdit()) {
            m_comboArchitecture->lineEdit()->setText(m_comboArchitecture->itemText(0));
            m_comboArchitecture->lineEdit()->setStyleSheet(QStringLiteral("color: #1e1e1e !important; background-color: #ffffff !important;"));
        }

        // Обновляем курсивное описание под комбобоксом
        if (m_lblArchDesc && !m_currentArchDescs.isEmpty()) {
            m_lblArchDesc->setText(m_currentArchDescs.at(0));
        }
    }

    // 3. ФОРСИРОВАННЫЙ СИСТЕМНЫЙ ПИНГ ОТРИСОВКИ ХОЛСТА QT6
    m_comboArchitecture->setEnabled(true);
    m_comboArchitecture->setVisible(true);
    m_comboArchitecture->update();

    if (m_comboArchitecture->parentWidget()) {
        m_comboArchitecture->parentWidget()->update();
    }
}

void SpeedPanel::onArchitectureChanged(int index) {
    if (!m_lblArchDesc || index < 0 || index >= m_currentArchDescs.size()) return;
    m_lblArchDesc->setText(m_currentArchDescs.at(index));
}

void SpeedPanel::verifyAndUnlockPipeline() {
    int signalsCount = getRealFileCount(QStringLiteral("sensors"));
    if (signalsCount == 0) {
        m_lblPipelineStatus->setText(QStringLiteral("<font color='red'> Ошибка ПАК: Файл simulink_motor_signals.csv не найден в директории!</font>"));
        return;
    }
    m_lblPipelineStatus->setText(QStringLiteral("<font color='#2ecc71'><b>✓ Конвейер наблюдателя скорости успешно верифицирован и готов!</b></font>"));
    m_btnActivate->setEnabled(false);
    m_btnActivate->setText(QStringLiteral("[ КОНВЕЙЕР НАБЛЮДАТЕЛЯ АКТИВЕН ]"));
    m_btnActivate->setStyleSheet(QStringLiteral("background-color: #27ae60; color: white; font-weight: bold; height: 35px; border-radius: 4px;"));

    emit pipelineActivated();
}

bool SpeedPanel::saveFieldsToYaml(const QString& projectPath) {
    if (m_widgetsMap.isEmpty()) return false;
    QString targetYamlPath = projectPath + QStringLiteral("/config/hyperparameters.yaml");
    QDir().mkpath(projectPath + QStringLiteral("/config"));

    QStringList trainingBlock;
    QStringList hardwareBlock;
    QStringList loggingBlock;

    QMap<QString, QWidget*>::const_iterator i = m_widgetsMap.constBegin();
    while (i != m_widgetsMap.constEnd()) {
        QString name = i.key();
        QWidget* widget = i.value();
        QString valueStr;

        if (QSpinBox* sb = qobject_cast<QSpinBox*>(widget)) valueStr = QString::number(sb->value());
        else if (QDoubleSpinBox* dsb = qobject_cast<QDoubleSpinBox*>(widget)) valueStr = QString::number(dsb->value());
        else if (QComboBox* cb = qobject_cast<QComboBox*>(widget)) valueStr = QString("'%1'").arg(cb->currentText());
        else if (QCheckBox* chb = qobject_cast<QCheckBox*>(widget)) valueStr = chb->isChecked() ? "true" : "false";
        else if (QLineEdit* le = qobject_cast<QLineEdit*>(widget)) valueStr = QString("'%1'").arg(le->text().trimmed());

        if (name == "epochs" || name == "batch_size" || name == "learning_rate" || name == "optimizer") {
            if (name == "batch_size") valueStr.remove(QStringLiteral("'"));
            trainingBlock << QString("  %1: %2").arg(name, valueStr);
        } else if (name == "device" || name == "num_workers" || name == "mixed_precision") {
            if (name == "device") valueStr = valueStr.toLower();
            hardwareBlock << QString("  %1: %2").arg(name, valueStr);
        } else if (name == "checkpoint_frequency" || name == "monitor_metric") {
            loggingBlock << QString("  %1: %2").arg(name, valueStr);
        }
        ++i;
    }

    QString archId = m_comboArchitecture ? m_comboArchitecture->currentData().toString() : QStringLiteral("medium");
    QString visMode = m_widgetsMap.contains(QStringLiteral("speed_visualization_mode")) ?
                          qobject_cast<QComboBox*>(m_widgetsMap[QStringLiteral("speed_visualization_mode")])->currentText() : QStringLiteral("comparative");

    QStringList yamlLines;
    yamlLines << QStringLiteral("training:");
    yamlLines << trainingBlock;
    yamlLines << QStringLiteral("hardware:");
    yamlLines << hardwareBlock;
    yamlLines << QStringLiteral("logging_and_save:");
    yamlLines << loggingBlock;

    yamlLines << QStringLiteral("mlops_pipeline:");
    yamlLines << QString("  control_mode: 'rotation_speed_signals'");
    yamlLines << QString("  model_architecture: '%1'").arg(archId);
    yamlLines << QString("  speed_visualization_mode: '%1'").arg(visMode);

    QFile file(targetYamlPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << yamlLines.join("\n");
        file.close();
        return true;
    }
    return false;
}

bool SpeedPanel::loadFieldsFromYaml(const QString& projectPath) {
    QString projectYamlPath = projectPath + QStringLiteral("/config/hyperparameters.yaml");
    QFile file(projectYamlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    this->blockSignals(true);
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line.contains(':')) {
            QString key = line.section(':', 0, 0).trimmed();
            QString value = line.section(':', 1).trimmed();
            if (value.startsWith('\'') && value.endsWith('\'')) value = value.mid(1, value.length() - 2);

            if (m_widgetsMap.contains(key)) {
                QWidget* widget = m_widgetsMap[key];
                if (QSpinBox* sb = qobject_cast<QSpinBox*>(widget)) sb->setValue(value.toInt());
                else if (QDoubleSpinBox* dsb = qobject_cast<QDoubleSpinBox*>(widget)) dsb->setValue(value.toDouble());
                else if (QComboBox* cb = qobject_cast<QComboBox*>(widget)) cb->setCurrentText(value);
                else if (QCheckBox* chb = qobject_cast<QCheckBox*>(widget)) chb->setChecked(value == "true");
                else if (QLineEdit* le = qobject_cast<QLineEdit*>(widget)) le->setText(value);
            }
            else if (key == "model_architecture" && m_comboArchitecture) {
                m_comboArchitecture->setCurrentIndex(m_comboArchitecture->findData(value));
            }
        }
    }
    file.close();
    this->blockSignals(false);
    return true;
}