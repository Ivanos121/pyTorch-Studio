#include "ai_panel.h"
#include "neuro_programm.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QCoreApplication>
#include <QFrame>

AI_panel::AI_panel(QWidget *parent)
    : QWidget(parent)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(15);

    // =========================================================================
    // СМАРТ-ТАЙМЕР АВТОСОХРАНЕНИЯ (СБРАСЫВАЕТ АКТИВАЦИЮ ПАК ПРИ ИЗМЕНЕНИИ)
    // =========================================================================
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    connect(m_saveTimer, &QTimer::timeout, this, [this]() {
        if (this->wf) {
            QString currentPath = this->wf->getCurrentProjectPath();
            if (currentPath.isEmpty()) {
                currentPath = QCoreApplication::applicationDirPath();
            }
            this->saveFieldsToYaml(currentPath);
        }

        if (m_btnActivate && m_lblPipelineStatus) {
            m_lblPipelineStatus->setText(QStringLiteral("⏳ Настройки автосохранены. Требуется повторная верификация ПАК."));
            m_btnActivate->setEnabled(true);
            m_btnActivate->setText(QStringLiteral("🔓 АКТИВИРОВАТЬ КОНВЕЙЕР ОБУЧЕНИЯ (ПОЛНАЯ ПРОВЕРКА)"));
            m_btnActivate->setStyleSheet(QStringLiteral("background-color: #2980b9; color: white; font-weight: bold; height: 35px; border-radius: 4px;"));
        }
    });
}

void AI_panel::clearLayout(QLayout* layout) {
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
bool AI_panel::buildUiFromConfig(const QString& schemaPath) {
    if (!m_mainLayout) return false;

    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    clearLayout(m_mainLayout);
    m_widgetsMap.clear();

    QFile file(schemaPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << " [DynamicUI]: Файл схемы конфигурации не найден:" << schemaPath;
        return false;
    }
    QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    file.close();

    // ИНИЦИАЛИЗАЦИЯ КОНТЕЙНЕРА ПРОКРУТКИ SCROLL AREA
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_scrollContentWidget = new QWidget(scrollArea);

    QVBoxLayout *containerLayout = new QVBoxLayout(m_scrollContentWidget);
    containerLayout->setContentsMargins(10, 10, 10, 10);
    containerLayout->setSpacing(15);

    // СЕТКА ДЛЯ ВЕРХНЕГО ЭТАЖА (Блоки 50/50)
    QWidget *topFloorWidget = new QWidget(m_scrollContentWidget);
    QGridLayout *gridLayout = new QGridLayout(topFloorWidget);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(15);
    containerLayout->addWidget(topFloorWidget);

    QMap<QString, QFormLayout*> groupsMap;

    // Инициализируем два верхних GroupBox (Защита от деформаций)
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

    // Распределяем верхний этаж 50/50
    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 1);
    gridLayout->setColumnMinimumWidth(0, 200);
    gridLayout->setColumnMinimumWidth(1, 200);

    // ИНИЦИАЛИЗИРУЕМ НИЖНИЙ ЭТАЖ (Панель MLOps)
    QGroupBox* mlopsBox = new QGroupBox(QStringLiteral("Панель настройки конвейера MLOps"), m_scrollContentWidget);
    mlopsBox->setStyleSheet(QStringLiteral("QGroupBox { font-weight: bold; }"));
    QVBoxLayout* mlopsLayout = new QVBoxLayout(mlopsBox);
    mlopsLayout->setContentsMargins(12, 14, 12, 12);
    mlopsLayout->setSpacing(12);
    containerLayout->addWidget(mlopsBox);

    containerLayout->addStretch(1);

    // ПАРСИНГ И ДИНАМИЧЕСКАЯ СБОРКА ИЗ СХЕМЫ JSON
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject param = array[i].toObject();
        if (param.contains(QStringLiteral("meta_config"))) continue;

        QString groupID = param[QStringLiteral("group")].toString();
        QString name = param[QStringLiteral("name")].toString();
        QString labelText = param[QStringLiteral("label")].toString();
        QString type = param[QStringLiteral("type")].toString();

        if (groupID == QStringLiteral("logging")) {
            groupID = QStringLiteral("hardware");
        }

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
                dSpinBox->setSingleStep(0.001);
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
                mlopsLayout->addWidget(new QLabel(QString("<b>%1</b>").arg(labelText), m_scrollContentWidget));

                m_modeGroup = new QButtonGroup(m_scrollContentWidget);
                QJsonArray options = param["options"].toArray();

                QFrame *analysisFrame = new QFrame(m_scrollContentWidget);
                analysisFrame->setStyleSheet(QStringLiteral("background-color: #1e1e1e; border-radius: 4px; border: 1px solid #333;"));
                QVBoxLayout *analysisLayout = new QVBoxLayout(analysisFrame);

                int sensorsFiles = getRealFileCount(QStringLiteral("sensors"));
                int thermalFiles = getRealFileCount(QStringLiteral("thermal"));

                QLabel *lblSens = new QLabel(QString(" 📊 Временные ряды статора (sensors_csv): Найдено %1 логов").arg(sensorsFiles), m_scrollContentWidget);
                QLabel *lblTher = new QLabel(QString(" 📷 Теплограммы ИК-камеры (data/raw): Найдено %1 снимков").arg(thermalFiles), m_scrollContentWidget);

                lblSens->setStyleSheet(sensorsFiles > 0 ? QStringLiteral("color: #2ecc71; font-weight: bold;") : QStringLiteral("color: #e74c3c;"));
                lblTher->setStyleSheet(thermalFiles >= 10 ? QStringLiteral("color: #2ecc71; font-weight: bold;") : QStringLiteral("color: #f1c40f;"));

                analysisLayout->addWidget(lblSens);
                analysisLayout->addWidget(lblTher);
                mlopsLayout->addWidget(analysisFrame);

                for (int j = 0; j < options.size(); ++j) {
                    QJsonObject optObj = options[j].toObject();
                    QString optId = optObj["id"].toString();
                    QString optLabel = optObj["label"].toString();
                    int minFiles = optObj["min_files"].toInt();

                    QRadioButton *radio = new QRadioButton(optLabel, m_scrollContentWidget);
                    m_modeGroup->addButton(radio, j);
                    mlopsLayout->addWidget(radio);

                    int realFiles = (optId == "sensors") ? sensorsFiles : thermalFiles;
                    if (realFiles < minFiles) {
                        radio->setEnabled(false);
                        radio->setText(optLabel + QStringLiteral(" [ДАННЫХ НЕДОСТАТОЧНО]"));
                    }
                }
                m_modeGroup->button(0)->setChecked(true);

                connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
                    QString modeId = (id == 0) ? QStringLiteral("sensors") : ((id == 1) ? QStringLiteral("thermal") : QStringLiteral("hybrid"));
                    updateArchitectureMapping(modeId, m_modelArchFieldObj);
                    triggerAutoSave();
                });
            }
            else if (type == "dynamic_enum") {
                m_modelArchFieldObj = param;
                mlopsLayout->addWidget(new QLabel(QString("<b>%1</b>").arg(labelText), m_scrollContentWidget));

                m_comboArchitecture = new QComboBox(m_scrollContentWidget);
                m_lblArchDesc = new QLabel(m_scrollContentWidget);
                m_lblArchDesc->setStyleSheet(QStringLiteral("color: #7f8c8d; font-style: italic;"));

                mlopsLayout->addWidget(m_comboArchitecture);
                mlopsLayout->addWidget(m_lblArchDesc);

                updateArchitectureMapping(QStringLiteral("sensors"), m_modelArchFieldObj);
                connect(m_comboArchitecture, &QComboBox::currentIndexChanged, this, &AI_panel::triggerAutoSave);
            }
            else if (type == "action_button") {
                mlopsLayout->addSpacing(10);
                m_lblPipelineStatus = new QLabel(QStringLiteral("💡 Статус ПАК: Конфигурация готова. Требуется верификация."), m_scrollContentWidget);
                mlopsLayout->addWidget(m_lblPipelineStatus);

                m_btnActivate = new QPushButton(labelText, m_scrollContentWidget);
                m_btnActivate->setStyleSheet(QStringLiteral("background-color: #2980b9; color: white; font-weight: bold; height: 35px; border-radius: 4px;"));

                connect(m_btnActivate, &QPushButton::clicked, this, &AI_panel::verifyAndUnlockPipeline);
                mlopsLayout->addWidget(m_btnActivate);
            }
        }
    }

    scrollArea->setWidget(m_scrollContentWidget);
    m_mainLayout->addWidget(scrollArea);
    this->update();
    return true;
}

void AI_panel::setupAutoSaveTriggers(QWidget* widget, const QString& type) {
    if (type == "int") connect(qobject_cast<QSpinBox*>(widget), &QSpinBox::valueChanged, this, &AI_panel::triggerAutoSave);
    else if (type == "double") connect(qobject_cast<QDoubleSpinBox*>(widget), &QDoubleSpinBox::valueChanged, this, &AI_panel::triggerAutoSave);
    else if (type == "enum") connect(qobject_cast<QComboBox*>(widget), &QComboBox::currentIndexChanged, this, &AI_panel::triggerAutoSave);
    else if (type == "bool") connect(qobject_cast<QCheckBox*>(widget), &QCheckBox::toggled, this, &AI_panel::triggerAutoSave);
    else if (type == "text") connect(qobject_cast<QLineEdit*>(widget), &QLineEdit::textChanged, this, &AI_panel::triggerAutoSave);
}

void AI_panel::triggerAutoSave() {
    if (m_saveTimer) m_saveTimer->start(3000);
}

int AI_panel::getRealFileCount(const QString &modeId) {
    // 1. СИЛОВОЙ ФИЛЬТР-ОЧИСТИТЕЛЬ РАБОЧЕГО ПУТИ ОТ ПАПКИ СБОРКИ BUILD
    QString rootPath = QCoreApplication::applicationDirPath();
    if (rootPath.contains(QStringLiteral("/build"))) {
        rootPath = QStringLiteral("/home/elf/zcc/z1"); // Жесткий эталонный путь к проекту [INDEX_0.1.10]
    }

    // 2. НАСТРАИВАЕМ РЕГИСТРОНЕЗАВИСИМЫЕ ФИЛЬТРЫ РАСШИРЕНИЙ ФАЙЛОВ
    QStringList filters;
    filters << QStringLiteral("*.jpg")  << QStringLiteral("*.jpeg")
            << QStringLiteral("*.JPG")  << QStringLiteral("*.JPEG")
            << QStringLiteral("*.csv")  << QStringLiteral("*.CSV");

    // 3. АБСОЛЮТНЫЙ ПОДСЧЕТ ФАЙЛОВ В ДИРЕКТОРИЯХ LINUX
    if (modeId == QStringLiteral("sensors")) {
        QString sensorsPath = rootPath + QStringLiteral("/datasets/sensors_csv");
        return QDir(sensorsPath).entryList(filters, QDir::Files).count();
    }

    // Для ИК-контроля и Гибрида сканируем универсальные подпапки normal и overheat
    QString normalPath   = rootPath + QStringLiteral("/data/raw/normal");
    QString overheatPath = rootPath + QStringLiteral("/data/raw/overheat");

    int normalCount   = QDir(normalPath).entryList(filters, QDir::Files).count();
    int overheatCount = QDir(overheatPath).entryList(filters, QDir::Files).count();

    return normalCount + overheatCount;
}

void AI_panel::updateArchitectureMapping(const QString &modeId, const QJsonObject &fieldObj) {
    if (!m_comboArchitecture) return;

    m_comboArchitecture->clear();
    m_currentArchDescs.clear(); // Очищаем старый кэш описаний

    QJsonArray arr = fieldObj["mapping"].toObject().value(modeId).toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject archObj = arr[i].toObject();
        m_comboArchitecture->addItem(archObj["label"].toString(), archObj["id"].toString());
        m_currentArchDescs.append(archObj["desc"].toString()); // Кэшируем описание
    }

    // БЕЗОПАСНЫЙ СВЯЗУЮЩИЙ ШАГ ДЛЯ Qt 6 (Указатель на метод класса + UniqueConnection)
    connect(m_comboArchitecture, &QComboBox::currentIndexChanged,
            this, &AI_panel::onArchitectureChanged, Qt::UniqueConnection);

    if (m_comboArchitecture->count() > 0) {
        m_comboArchitecture->setCurrentIndex(0);
        onArchitectureChanged(0); // Принудительно обновляем текст для первой сети
    }
}


void AI_panel::verifyAndUnlockPipeline() {
    int checkedId = m_modeGroup->checkedId();
    QString modeId = (checkedId == 0) ? QStringLiteral("sensors") : ((checkedId == 1) ? QStringLiteral("thermal") : QStringLiteral("hybrid"));

    int sensorsCount = getRealFileCount(QStringLiteral("sensors"));
    int thermalCount = getRealFileCount(QStringLiteral("thermal"));

    if (modeId == "hybrid" && (sensorsCount == 0 || thermalCount < 10)) {
        m_lblPipelineStatus->setText(QStringLiteral("<font color='red'>❌ Ошибка ПАК: Для гибридного режима нужны лог токов и ИК-снимки!</font>"));
        return;
    }

    m_lblPipelineStatus->setText(QStringLiteral("<font color='#2ecc71'><b>✓ Конвейер ПАК успешно активирован и готов к запуску!</b></font>"));
    m_btnActivate->setEnabled(false);
    m_btnActivate->setText(QStringLiteral("[ 🔒 КОНВЕЙЕР ПАК УТВЕРЖДЕН И АКТИВЕН ]"));
    m_btnActivate->setStyleSheet(QStringLiteral("background-color: #27ae60; color: white; font-weight: bold; height: 35px; border-radius: 4px;"));

    emit pipelineActivated();
}

bool AI_panel::saveFieldsToYaml(const QString& projectPath) {
    if (m_widgetsMap.isEmpty()) return false;

    QString targetProjectPath = projectPath.trimmed();
    if (targetProjectPath.contains(QStringLiteral("/build/")) || targetProjectPath.endsWith(QStringLiteral("/build"))) {
        if (targetProjectPath.contains(QStringLiteral("pyTorch-Studio"))) {
            targetProjectPath = QStringLiteral("/home/elf/zcc/z1");
        } else {
            int buildIdx = targetProjectPath.indexOf(QStringLiteral("/build"));
            if (buildIdx != -1) targetProjectPath = targetProjectPath.left(buildIdx);
        }
    }

    if (targetProjectPath.isEmpty() || targetProjectPath.length() < 3) {
        targetProjectPath = QStringLiteral("/home/elf/zcc/z1");
    }

    QString finalConfigDir = targetProjectPath + QStringLiteral("/config");
    QDir().mkpath(finalConfigDir);
    QString targetYamlPath = finalConfigDir + QStringLiteral("/hyperparameters.yaml");

    // Инициализация списков блоков YAML
    QStringList trainingBlock;
    QStringList hardwareBlock;
    QStringList loggingBlock;
    QStringList hfBlock;
    QStringList mlopsBlock;
    QStringList verificationBlock; // ДОБАВЛЕНО: Секция верификации ПАК

    QMap<QString, QWidget*>::const_iterator i = m_widgetsMap.constBegin();
    while (i != m_widgetsMap.constEnd()) {
        QString name = i.key();
        QWidget* widget = i.value();
        QString valueStr;

        if (QSpinBox* sb = qobject_cast<QSpinBox*>(widget)) {
            valueStr = QString::number(sb->value());
        } else if (QDoubleSpinBox* dsb = qobject_cast<QDoubleSpinBox*>(widget)) {
            valueStr = QString::number(dsb->value());
        } else if (QComboBox* cb = qobject_cast<QComboBox*>(widget)) {
            valueStr = QString("'%1'").arg(cb->currentText());
        } else if (QCheckBox* chb = qobject_cast<QCheckBox*>(widget)) {
            valueStr = chb->isChecked() ? "true" : "false";
        } else if (QLineEdit* le = qobject_cast<QLineEdit*>(widget)) {
            valueStr = QString("'%1'").arg(le->text().trimmed());
        }

        // Распределяем параметры по блокам манифеста
        if (name == "epochs" || name == "batch_size" || name == "learning_rate" || name == "optimizer") {
            // ИСПРАВЛЕНО: Очищаем 'batch_size' от ложных кавычек комбобокса, переводя в Integer
            if (name == "batch_size") {
                valueStr.remove(QStringLiteral("'"));
            }
            trainingBlock << QString("  %1: %2").arg(name, valueStr);
        } else if (name == "device" || name == "num_workers" || name == "mixed_precision") {
            if (name == "device") valueStr = valueStr.toLower();
            hardwareBlock << QString("  %1: %2").arg(name, valueStr);
        } else if (name == "checkpoint_frequency" || name == "monitor_metric") {
            loggingBlock << QString("  %1: %2").arg(name, valueStr);
        } else if (name == "push_to_hub" || name == "repo_id") {
            hfBlock << QString("  %1: %2").arg(name, valueStr);
        }
        ++i;
    }

    // Парсинг активной задачи и выбранного селектора архитектур нейросетей
    int checkedId = m_modeGroup ? m_modeGroup->checkedId() : 0;
    QString modeId = (checkedId == 0) ? "sensors" : ((checkedId == 1) ? "thermal" : "hybrid");
    QString archId = m_comboArchitecture ? m_comboArchitecture->currentData().toString() : QStringLiteral("thermal_gru");

    mlopsBlock << QString("  control_mode: '%1'").arg(modeId);
    mlopsBlock << QString("  model_architecture: '%1'").arg(archId);

    // ДОБАВЛЕНО: Сквозное автоматическое наполнение секции верификации под часовой инференс
    verificationBlock << QStringLiteral("  target_sensor_index: 2"); // 2 - совмещенный лог Статор + Ротор
    verificationBlock << QStringLiteral("  window_size: 900");        // История скользящего окна
    verificationBlock << QStringLiteral("  step_size: 300");

    // Формируем итоговую текстовую структуру YAML-документа
    QStringList yamlLines;
    yamlLines << QStringLiteral("# =========================================================================");
    yamlLines << QStringLiteral("# AUTOMATED HYBRID MULTI-TASK PYTORCH STUDIO MANIFEST CONFIGURATION");
    yamlLines << QStringLiteral("# =========================================================================");
    yamlLines << QStringLiteral("training:") << trainingBlock.join("\n") << QStringLiteral("");
    yamlLines << QStringLiteral("hardware:") << hardwareBlock.join("\n") << QStringLiteral("");
    yamlLines << QStringLiteral("logging_and_save:") << loggingBlock.join("\n") << QStringLiteral("");

    // ИНТЕГРИРОВАНО: Запекаем секцию верификации в итоговый массив строк
    yamlLines << QStringLiteral("verification:") << verificationBlock.join("\n") << QStringLiteral("");

    yamlLines << QStringLiteral("mlops_pipeline:") << mlopsBlock.join("\n") << QStringLiteral("");
    yamlLines << QStringLiteral("paths:");
    yamlLines << QStringLiteral("  train_dir: 'datasets/training'");
    yamlLines << QStringLiteral("  val_dir: 'datasets/validate'") << QStringLiteral("");
    yamlLines << QStringLiteral("huggingface:") << hfBlock.join("\n");

    QFile file(targetYamlPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        // Модернизировано под актуальный стандарт энкодеров Qt6
        out.setEncoding(QStringConverter::Utf8);
        out << yamlLines.join("\n");
        file.close();

        if (this->wf) {
            this->wf->updateStatusLogText(QStringLiteral("Панель ИИ: Конфигурация MLOps манифеста обновлена"), QStringLiteral("#4caf50"));
            QTimer::singleShot(3500, this, [this]() {
                if (this->wf) this->wf->updateStatusLogText(QStringLiteral("Jedi: Готов к работе"), QStringLiteral("#ef5350"));
            });
        }
        return true;
    }
    return false;
}

void AI_panel::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    QTimer::singleShot(50, this, [this]() {
        if (m_scrollContentWidget && m_mainLayout) {
            m_scrollContentWidget->updateGeometry();
            if (m_scrollContentWidget->layout()) {
                m_scrollContentWidget->layout()->invalidate();
                m_scrollContentWidget->layout()->activate();
            }
            m_mainLayout->invalidate();
            m_mainLayout->activate();
            this->adjustSize();
            this->update();
        }
    });
}

bool AI_panel::loadFieldsFromYaml(const QString& projectPath) {
    if (projectPath.isEmpty()) return false;
    QString projectYamlPath = projectPath + QStringLiteral("/config/hyperparameters.yaml");
    QString appCacheYamlPath = QCoreApplication::applicationDirPath() + QStringLiteral("/config/hyperparameters.yaml");
    QString finalLoadPath = QFile::exists(projectYamlPath) ? projectYamlPath : appCacheYamlPath;

    QFile file(finalLoadPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    this->blockSignals(true);
    if (m_modeGroup) m_modeGroup->blockSignals(true);
    if (m_comboArchitecture) m_comboArchitecture->blockSignals(true);

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
                else if (QDoubleSpinBox* dsb = qobject_cast<QDoubleSpinBox*>(widget)) { value.replace(',', '.'); dsb->setValue(value.toDouble()); }
                else if (QComboBox* cb = qobject_cast<QComboBox*>(widget)) { if (key == "device") value = value.toUpper(); cb->setCurrentText(value); }
                else if (QCheckBox* chb = qobject_cast<QCheckBox*>(widget)) chb->setChecked(value == "true");
                else if (QLineEdit* le = qobject_cast<QLineEdit*>(widget)) le->setText(value);
            }
            else if (key == "control_mode" && m_modeGroup) {
                int id = (value == "sensors") ? 0 : ((value == "thermal") ? 1 : 2);
                if (m_modeGroup->button(id) && m_modeGroup->button(id)->isEnabled()) {
                    m_modeGroup->button(id)->setChecked(true);
                    updateArchitectureMapping(value, m_modelArchFieldObj);
                }
            }
            else if (key == "model_architecture" && m_comboArchitecture) {
                m_comboArchitecture->setCurrentIndex(m_comboArchitecture->findData(value));
            }
        }
    }
    file.close();

    this->blockSignals(false);
    if (m_modeGroup) m_modeGroup->blockSignals(false);
    if (m_comboArchitecture) m_comboArchitecture->blockSignals(false);

    if (finalLoadPath == appCacheYamlPath) this->saveFieldsToYaml(projectPath);
    return true;
}

void AI_panel::onArchitectureChanged(int index) {
    if (!m_lblArchDesc || index < 0 || index >= m_currentArchDescs.size()) return;

    // Безопасно обновляем интерфейс из кэша строк
    m_lblArchDesc->setText(m_currentArchDescs.at(index));
}

