#include "dynamicformbuilder.h"

DynamicFormBuilder& DynamicFormBuilder::instance() {
    static DynamicFormBuilder inst;
    return inst;
}

void DynamicFormBuilder::clearLayout(QLayout* layout) {
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

bool DynamicFormBuilder::buildLayoutFromJson(const QString& schemaPath, QVBoxLayout* mainLayout) {
    if (!mainLayout) return false;

    // Полностью очищаем старый слой интерфейса и старую карту виджетов
    clearLayout(mainLayout);
    m_widgetsMap.clear();

    // =========================================================================
    // ШАГ 1: Читаем динамический конфиг ТЕКУЩИХ ЗНАЧЕНИЙ пользователя
    // =========================================================================
    QJsonObject currentValues;
    // Путь к файлу живых значений (можно вынести в параметры или константы)
    QFile valuesFile(QStringLiteral("config/logging_master.json"));
    if (valuesFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(valuesFile.readAll());
        if (doc.isObject()) {
            currentValues = doc.object(); // Содержит пары "Имя_Ключа": значение
        }
        valuesFile.close();
    } else {
        qWarning() << " [DynamicUI ПРЕДУПРЕЖДЕНИЕ]: logging_master.json не найден. Используем дефолты схемы.";
    }

    // =========================================================================
    // ШАГ 2: Читаем основной файл СХЕМЫ (Манифест-шаблон)
    // =========================================================================
    QFile file(schemaPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << " [DynamicUI ОШИБКА]: Не удалось открыть файл схемы:" << schemaPath;
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        qWarning() << " [DynamicUI ОШИБКА]: Неверный формат JSON-схемы. Ожидался массив.";
        return false;
    }

    QJsonArray array = doc.array();
    QMap<QString, QFormLayout*> groupsMap;

    for (int i = 0; i < array.size(); ++i) {
        QJsonObject param = array[i].toObject();

        // В вашей схеме используются ключи "category" и "key" вместо "group" и "name"
        QString groupName = param["category"].toString();
        QString name      = param["key"].toString();
        QString labelText = param["label"].toString();
        QString type      = param["type"].toString();

        // Динамическое создание QGroupBox под каждую группу из JSON, если она еще не создана
        if (!groupsMap.contains(groupName)) {
            QGroupBox* groupBox = new QGroupBox(groupName);
            groupBox->setStyleSheet(QStringLiteral("QGroupBox { font-weight: bold; }"));
            QFormLayout* formLayout = new QFormLayout(groupBox);
            formLayout->setLabelAlignment(Qt::AlignLeft);
            groupsMap[groupName] = formLayout;
            mainLayout->addWidget(groupBox);
        }

        QFormLayout* currentLayout = groupsMap[groupName];

        // --- ВЫЧИСЛЯЕМ ЖИВОЕ ЗНАЧЕНИЕ ДЛЯ ДАННОГО КЛЮЧА ---
        bool hasSavedValue = currentValues.contains(name);
        QJsonValue userValue = currentValues.value(name);
        QJsonValue defaultValue = param["default"];

        // =========================================================================
        // ШАГ 3: Фабрика виджетов с подстановкой живых значений
        // =========================================================================
        if (type == "int") {
            QSpinBox* spinBox = new QSpinBox();
            spinBox->setRange(param["min"].toInt(0), param["max"].toInt(1000));

            // Если есть сохраненное значение — берем его, иначе — дефолт из схемы
            int finalVal = hasSavedValue ? userValue.toInt() : defaultValue.toInt(10);
            spinBox->setValue(finalVal);

            currentLayout->addRow(labelText, spinBox);
            m_widgetsMap[name] = spinBox;
        }
        else if (type == "double") {
            QDoubleSpinBox* dSpinBox = new QDoubleSpinBox();
            dSpinBox->setRange(param["min"].toDouble(0.0), param["max"].toDouble(1.0));

            double finalVal = hasSavedValue ? userValue.toDouble() : defaultValue.toDouble(0.001);
            dSpinBox->setValue(finalVal);

            dSpinBox->setDecimals(param["decimals"].toInt(5));
            dSpinBox->setSingleStep(0.001);
            currentLayout->addRow(labelText, dSpinBox);
            m_widgetsMap[name] = dSpinBox;
        }
        else if (type == "enum") {
            QComboBox* comboBox = new QComboBox();
            QJsonArray options = param["options"].toArray();
            for (int j = 0; j < options.size(); ++j) {
                comboBox->addItem(options[j].toString());
            }

            // Для enum: если сохранено число (индекс), ставим по индексу, иначе по тексту дефолта
            if (hasSavedValue) {
                comboBox->setCurrentIndex(userValue.toInt(0));
            } else {
                // Если дефолт задан числом
                if (defaultValue.isDouble()) {
                    comboBox->setCurrentIndex(defaultValue.toInt(0));
                } else {
                    comboBox->setCurrentText(defaultValue.toString());
                }
            }

            currentLayout->addRow(labelText, comboBox);
            m_widgetsMap[name] = comboBox;
        }
        else if (type == "bool") {
            QCheckBox* checkBox = new QCheckBox();

            bool finalVal = hasSavedValue ? userValue.toBool() : defaultValue.toBool(false);
            checkBox->setChecked(finalVal);

            currentLayout->addRow(labelText, checkBox);
            m_widgetsMap[name] = checkBox;
        }
        else if (type == "string" || type == "text") { // Добавлена поддержка обоих типов строк
            QLineEdit* lineEdit = new QLineEdit();

            QString finalVal = hasSavedValue ? userValue.toString() : defaultValue.toString();
            lineEdit->setText(finalVal);

            currentLayout->addRow(labelText, lineEdit);
            m_widgetsMap[name] = lineEdit;
        }
    }

    mainLayout->addStretch(); // Плотная компоновка элементов кверху
    qDebug() << " [DynamicUI УСПЕХ]: Двухуровневый интерфейс успешно собран! Элементов в карте: " << m_widgetsMap.size();
    return true;
}

bool DynamicFormBuilder::saveFieldsToYaml(const QString& projectPath) {
    if (projectPath.isEmpty() || m_widgetsMap.isEmpty()) return false;

    // Группируем выходные данные под YAML-стандарт
    QStringList trainingBlock;
    QStringList hardwareBlock;
    QStringList loggingBlock;
    QStringList hfBlock;

    QMap<QString, QWidget*>::const_iterator i = m_widgetsMap.constBegin();
    while (i != m_widgetsMap.constEnd()) {
        QString name = i.key();
        QWidget* widget = i.value();
        QString valueStr;

        // Извлекаем значение в зависимости от реального класса виджета
        if (QSpinBox* sb = qobject_cast<QSpinBox*>(widget)) {
            valueStr = QString::number(sb->value());
        } else if (QDoubleSpinBox* dsb = qobject_cast<QDoubleSpinBox*>(widget)) {
            valueStr = QString::number(dsb->value());
        } else if (QComboBox* cb = qobject_cast<QComboBox*>(widget)) {
            valueStr = QString("'%1'").arg(cb->currentText());
        } else if (QCheckBox* chb = qobject_cast<QCheckBox*>(widget)) {
            valueStr = chb->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (QLineEdit* le = qobject_cast<QLineEdit*>(widget)) {
            valueStr = QString("'%1'").arg(le->text().trimmed());
        }

        // Автоматически распределяем переменные по блокам на основе их имен
        if (name == "epochs" || name == "batch_size" || name == "learning_rate" || name == "optimizer") {
            trainingBlock << QString("  %1: %2").arg(name, valueStr);
        } else if (name == "device" || name == "num_workers" || name == "mixed_precision") {
            hardwareBlock << QString("  %1: %2").arg(name, valueStr);
        } else if (name == "checkpoint_frequency" || name == "monitor_metric") {
            loggingBlock << QString("  %1: %2").arg(name, valueStr);
        } else if (name == "push_to_hub" || name == "repo_id") {
            hfBlock << QString("  %1: %2").arg(name, valueStr);
        }
        ++i;
    }

    // Собираем итоговую валидную структуру YAML-паспорта
    QStringList yamlLines;
    yamlLines << "# =========================================================================";
    yamlLines << "# AUTOMATED DATA-DRIVEN PYTORCH STUDIO HYPERPARAMETERS CONFIGURATION";
    yamlLines << "# =========================================================================";
    yamlLines << "training:" << trainingBlock.join(QStringLiteral("\n")) << "";
    yamlLines << "hardware:" << hardwareBlock.join(QStringLiteral("\n")) << "";
    yamlLines << "logging_and_save:" << loggingBlock.join(QStringLiteral("\n")) << "";
    yamlLines << "paths:";
    yamlLines << "  train_dir: 'datasets/training'";
    yamlLines << "  val_dir: 'datasets/validate'" << "";
    yamlLines << "huggingface:" << hfBlock.join(QStringLiteral("\n"));

    QString configDir = projectPath + QStringLiteral("/config");
    QDir().mkpath(configDir);

    QFile file(configDir + QStringLiteral("/hyperparameters.yaml"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << yamlLines.join(QStringLiteral("\n"));
        file.close();
        qDebug() << " [MLOps УСПЕХ]: Новая Data-Driven конфигурация записана на диск!";
        return true;
    }
    return false;
}
