#include "sessiontablewidget.h"

SessionTableWidget::SessionTableWidget(QWidget *parent) : QWidget(parent) {
    this->setupUi();
}

void SessionTableWidget::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Рождение жесткого QTableWidget
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setObjectName(QStringLiteral("internalSessionTableWidget"));

    // Настраиваем поведение выделения строк (без триггеров редактирования ячеек)
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Оформление под общую темную тему вашей Студии
    m_tableWidget->setStyleSheet(QStringLiteral(
        "QTableWidget { background-color: #1e1e1e; color: #d4d4d4; gridline-color: #333; border: none; font-family: monospace; }"
        "QHeaderView::section { background-color: #2d2d2d; color: #aaa; padding: 4px; border: 1px solid #3c3c3c; font-weight: bold; }"
        "QTableWidget::item:selected { background-color: #004b76; color: #fff; }"
        ));

    layout->addWidget(m_tableWidget);
}

void SessionTableWidget::setProjectPath(const QString &projectPath) {
    m_currentProjectPath = projectPath.trimmed();
    qDebug() << ">>> [MLOps ТАБЛИЦА]: Путь синхронизирован:" << m_currentProjectPath;
    this->refreshTable(); // Запуск динамической отрисовки
}

void SessionTableWidget::refreshTable() {
    // 1. Очищаем старые строки перед каждым свежим заполнением
    m_tableWidget->setRowCount(0);

    // === ШАГ 1: ХАРДКОРНОЕ ЧТЕНИЕ ТОЧНОГО КОНФИГА КОЛОНОК ===
    if (m_columns.isEmpty()) {
        // Прописываем тот самый 100% правильный путь к файлу схемы разметки
        QString configPath = QStringLiteral("/home/elf/pyTorch-Studio/Config/session_table_config.json");
        QFile fileConfig(configPath);
        bool configReadSuccess = false;

        if (fileConfig.open(QIODevice::ReadOnly)) {
            // Парсим json-массив полей
            QJsonArray arr = QJsonDocument::fromJson(fileConfig.readAll()).array();
            fileConfig.close();

            if (!arr.isEmpty()) {
                m_columns.clear();
                for (const QJsonValue &val : std::as_const(arr)) {
                    QJsonObject obj = val.toObject();

                    // Пропускаем невидимые колонки
                    if (!obj[QStringLiteral("visible")].toBool()) continue;

                    SessionColumn col;
                    col.key = obj[QStringLiteral("key")].toString();
                    col.header = obj[QStringLiteral("header")].toString();
                    col.width = obj[QStringLiteral("width")].toInt();
                    col.visible = true;
                    m_columns << col;
                }
                configReadSuccess = true;
                qDebug() << ">>> [MLOps ТАБЛИЦА] УСПЕХ: Схема колонок успешно считана из:" << configPath;
            }
        }

        // АВАРИЙНАЯ ПОДСТРАХОВКА: Если файл заблокирован ОС или поврежден,
        // принудительно инжектируем структуру в память, чтобы сетка не ломалась!
        if (!configReadSuccess || m_columns.isEmpty()) {
            qWarning() << "⚠️ [MLOps Таблица] ПРЕДУПРЕЖДЕНИЕ: Сбой чтения файла по пути:" << configPath
                       << ". Применение резервной структуры колонок.";
            m_columns = {
                {QStringLiteral("id"), QStringLiteral("Идентификатор сессии"), 260, true},
                {QStringLiteral("date"), QStringLiteral("Дата запуска"), 150, true},
                {QStringLiteral("optimizer"), QStringLiteral("Оптимизатор"), 100, true},
                {QStringLiteral("epochs"), QStringLiteral("Эпох"), 70, true},
                {QStringLiteral("val_mae"), QStringLiteral("Val MAE (°C)"), 100, true},
                {QStringLiteral("comment"), QStringLiteral("Комментарий к эксперименту"), -1, true}
            };
        }

        // Нарезаем сетку столбцов в QTableWidget под прочитанный конфиг полей
        m_tableWidget->setColumnCount(m_columns.count());
        QStringList headers;
        for (const auto &col : std::as_const(m_columns)) {
            headers << col.header;
        }
        m_tableWidget->setHorizontalHeaderLabels(headers);
    }

    // === АВТОНОМНЫЙ ПЕРЕХВАТ ПУТИ К ПРОЕКТУ Z1 (ДЛЯ ШАГА 2) ===
    if (m_currentProjectPath.isEmpty()) {
        QSettings settings(QStringLiteral("/home/elf/.config/PyTorchStudio/pystudio.conf"), QSettings::IniFormat);
        QString lastPath = settings.value(QStringLiteral("Projects/LastOpenedPath")).toString().trimmed();
        if (!lastPath.isEmpty()) m_currentProjectPath = lastPath;
    }
    if (m_currentProjectPath.isEmpty()) {
        QWidget *topWindow = this->window();
        if (topWindow) {
            QString parentPath = topWindow->property("currentOpenProjectPath").toString().trimmed();
            if (!parentPath.isEmpty()) m_currentProjectPath = parentPath;
        }
    }

    if (m_currentProjectPath.isEmpty()) {
        qWarning() << " [MLOps Таблица]: Отмена сканирования. Рабочий путь к проекту пуст.";
        return;
    }

    // === ШАГ 2: НАПОЛНЕНИЕ ЯЧЕЕК ИЗ JSON-ПАСПОРТОВ СЕССИЙ ===
    QDir metricsDir(QDir(m_currentProjectPath).filePath(QStringLiteral("metrics")));
    if (!metricsDir.exists()) return;

    QStringList sessionDirs = metricsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

    // Блокируем сигналы перерисовки на время массовой вставки для колоссального ускорения GUI
    m_tableWidget->blockSignals(true);

    for (const QString& sessionId : std::as_const(sessionDirs)) {
        QString jsonPath = metricsDir.filePath(sessionId + QStringLiteral("/session_meta.json"));
        QFile file(jsonPath);

        if (!file.exists()) continue;

        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();

            if (doc.isNull() || !doc.isObject()) continue;

            QJsonObject root = doc.object();
            QJsonObject hyper = root["hyperparameters"].toObject();
            QJsonObject metrics = root["metrics"].toObject();

            // Физически создаем строку в таблице
            int row = m_tableWidget->rowCount();
            m_tableWidget->insertRow(row);

            // Наполняем созданную строку ячейками строго по порядку из массива m_columns
            for (int colIndex = 0; colIndex < m_columns.count(); ++colIndex) {
                const auto &col = m_columns[colIndex];
                QString itemText = QStringLiteral("Н/Д");

                if (col.key == QStringLiteral("id")) {
                    itemText = sessionId;
                }
                else if (col.key == QStringLiteral("date")) {
                    QDateTime dateTime = QDateTime::fromString(root["timestamp"].toString(), Qt::ISODate);
                    itemText = dateTime.isValid() ? dateTime.toString("dd.MM.yyyy hh:mm:ss") : QStringLiteral("Н/Д");
                }
                else if (col.key == QStringLiteral("optimizer")) {
                    itemText = hyper["optimizer"].toString().toUpper().trimmed();
                    if (itemText.isEmpty()) itemText = QStringLiteral("ADAM");
                }
                else if (col.key == QStringLiteral("epochs")) {
                    itemText = QString::number(hyper["epochs"].toInt());
                }
                else if (col.key == QStringLiteral("val_mae")) {
                    double valMae = metrics["final_val_mae"].toDouble();
                    if (valMae == 0.0) valMae = metrics["stator_mae"].toDouble();
                    itemText = QString::number(valMae, 'f', 2);
                }
                else if (col.key == QStringLiteral("comment")) {
                    itemText = root["comment"].toString().trimmed();
                    if (itemText.isEmpty()) itemText = QStringLiteral("Без описания");
                }

                // Инициализируем ячейку, запрещаем её ручное редактирование в GUI и шьем в таблицу
                QTableWidgetItem *item = new QTableWidgetItem(itemText);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                m_tableWidget->setItem(row, colIndex, item);
            }
        }
    }

    // === ШАГ 3: АВТОМАТИЧЕСКАЯ ПОДГОНКА ГЕОМЕТРИИ КОЛОНОК ИЗ КОНФИГА ===
    for (int i = 0; i < m_columns.count(); ++i) {
        if (m_columns[i].width > 0) {
            m_tableWidget->setColumnWidth(i, m_columns[i].width);
        } else {
            // Если в JSON задан -1 (для комментария), растягиваем его во всю оставшуюся ширь страницы
            m_tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
        }
    }

    // Разблокируем сигналы модели и принудительно перерисовываем пиксели виджета
    m_tableWidget->blockSignals(false);
    m_tableWidget->update();
    this->updateGeometry(); // СИЛОВО заставляем Layout раскрыть виджет на весь экран
}

