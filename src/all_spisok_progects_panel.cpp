#include "all_spisok_progects_panel.h"
#include "statuscomboboxdelegate.h"
#include "projectindicatorsproxymodel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpacerItem>
#include <QSqlError>
#include <QHeaderView>
#include <QDebug>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QSqlQuery>
#include <QSettings>
#include <QEvent>
#include <QTimer>
#include <QMessageBox>
#include <QMenu>
#include <QDesktopServices>
#include <QProcess>
#include <QContextMenuEvent>

all_spisok_progects_panel::all_spisok_progects_panel(QWidget *parent)
    : QWidget(parent)
{
}

void all_spisok_progects_panel::initPanel(const QString& dbPath, const QJsonObject& layoutConfig)
{
    m_dbPath = dbPath;

    // 1. Обеспечиваем физическое существование каталога под базу данных Database/
    QFileInfo dbFileInfo(m_dbPath);
    QDir().mkpath(dbFileInfo.absolutePath());

    // 2. Инициализируем именованное соединение с SQLite
    m_db = QSqlDatabase::addDatabase("QSQLITE", "AllProjectsRegistryConnection");
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        qCritical() << "[all_spisok_progects_panel] Ошибка SQLite:" << m_db.lastError().text();
        return;
    }

    // 3. Создаем структуру таблицы проекта, если она запускается впервые
    QSqlQuery query(m_db);
    query.exec(
        "CREATE TABLE IF NOT EXISTS studio_projects ("
        "  project_name   TEXT PRIMARY KEY NOT NULL,"
        "  last_modified  TEXT NOT NULL,"
        "  project_status TEXT NOT NULL DEFAULT 'Создан',"
        "  dataset_size   TEXT NOT NULL DEFAULT '0 Кб',"
        "  project_path   TEXT NOT NULL"
        ");"
        );
    query.exec("CREATE INDEX IF NOT EXISTS idx_project_name ON studio_projects (project_name);");

    // 4. Настраиваем корневые свойства панели из JSON
    setObjectName(layoutConfig["widget_name"].toString());
    if (layoutConfig.contains("stylesheet")) {
        setStyleSheet(layoutConfig["stylesheet"].toString());
    }

    // 5. Запускаем рекурсивную сборку и отрисовку виджетов
    buildUiFromJson(layoutConfig);
}

bool all_spisok_progects_panel::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::Show) {
        this->refreshPanel();
    }

    // ИСПРАВЛЕНИЕ: Если мышь полностью покинула область панели проектов — сбрасываем курсор в дефолт
    if (event->type() == QEvent::Leave) {
        if (m_tableView) {
            m_tableView->setCursor(Qt::ArrowCursor);
        }
    }

    return QWidget::event(event);
}

void all_spisok_progects_panel::refreshPanel()
{
    // ============================================================================
    // АВТОМАТИЧЕСКАЯ ОЧИСТКА БАЗЫ ДАННЫХ ОТ «ЛИПОВЫХ» И СТЕРТЫХ СТРОК
    // ============================================================================
    QSqlQuery cleanQuery(m_db);
    if (cleanQuery.exec(QStringLiteral("SELECT project_path, project_name FROM studio_projects")))
    {
        QSqlQuery deleteQuery(m_db);
        while (cleanQuery.next()) {
            QString path = cleanQuery.value(0).toString();
            QString name = cleanQuery.value(1).toString();
            QFileInfo checkFile(path);

            // Если папки физически нет на жестком диске Linux — это мусор, удаляем из SQLite
            if (!checkFile.exists() || !checkFile.isDir()) {
                deleteQuery.prepare("DELETE FROM studio_projects WHERE project_path = :path");
                deleteQuery.bindValue(":path", path);
                deleteQuery.exec();
                qDebug() << "[all_spisok_progects_panel] Удален липовый проект из ведомости:" << name;
            }
        }
    }

    // ============================================================================
    // СЧИТЫВАНИЕ ВНЕШНИХ ПРОЕКТОВ ИЗ КОНФИГА PYTORCHSTUDIO
    // ============================================================================
    QSettings studioSettings(
        QStringLiteral("/home/elf/.config/PyTorchStudio/pystudio.conf"),
        QSettings::IniFormat
        );

    QString rawProjectsString = studioSettings.value(QStringLiteral("Main/recentProjectList")).toString();
    rawProjectsString.remove(QLatin1Char('"'));
    QStringList recentPaths = rawProjectsString.split(QLatin1Char(';'), Qt::SkipEmptyParts);

    QSqlQuery insertQuery(m_db);
    for (const QString& path : recentPaths) {
        QString cleanPath = path.trimmed();
        if (cleanPath.isEmpty()) continue;

        QFileInfo projectInfo(cleanPath);
        if (projectInfo.exists() && projectInfo.isDir()) {
            QString pureName = projectInfo.fileName(); // Используется имя pureName

            QSqlQuery checkQuery(m_db);
            checkQuery.prepare("SELECT COUNT(*) FROM studio_projects WHERE project_path = :path");
            checkQuery.bindValue(":path", cleanPath);

            if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() == 0) {
                insertQuery.prepare(
                    "INSERT INTO studio_projects (project_name, last_modified, project_status, dataset_size, project_path, is_pinned) "
                    "VALUES (:name, :mod, 'Создан', '0 Кб', :path, 0)"
                    );

                // ИСПРАВЛЕНИЕ: Используем переменную pureName вместо несуществующей projectName
                QString initialName = QString::fromUtf8("⚪ ") + pureName;

                insertQuery.bindValue(":name", initialName);
                insertQuery.bindValue(":mod", projectInfo.lastModified().toString("dd.MM.yyyy hh:mm"));
                insertQuery.bindValue(":path", cleanPath);
                insertQuery.exec();
            }
        }
    }

    // ВЫЗОВ ДВУХЭТАПНОГО СКАНИРОВАНИЯ ДИСКА ДЛЯ АКТУАЛИЗАЦИИ РАЗМЕРОВ
    scanProjectsDirectory(QStringLiteral("/home/elf/pyTorch-Studio/projects"));

    // ============================================================================
    // 6. ЖЕСТКИЙ СБРОС КЭША МОДЕЛИ ДЛЯ ОТОБРАЖЕНИЯ ОБНОВЛЕННЫХ ДАННЫХ НА ЭКРАНЕ
    // ============================================================================
    if (m_tableModel && m_tableView) {
        m_tableModel->revertAll();
        m_tableModel->setTable(QStringLiteral("studio_projects"));
        m_tableModel->setSort(5, Qt::DescendingOrder);
        m_tableModel->select();
        m_tableView->viewport()->update();

        // 1. Раздвигаем ширину столбцов под длинные текстовые мегабайты и счетчики ИИ
        m_tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

        // Принудительно выставляем просторную высоту 36px для каждого элемента на экране
        m_tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_tableView->verticalHeader()->setDefaultSectionSize(36);
        for (int i = 0; i < m_tableModel->rowCount(); ++i) {
            m_tableView->setRowHeight(i, 36);
        }

        m_tableView->setStyleSheet(QStringLiteral(
            "QTableView::item { padding-top: 4px; padding-bottom: 4px; padding-left: 8px; padding-right: 8px; }"
            ));

        // Навечно прячем только технический флаг is_pinned (индекс 5)
        m_tableView->setColumnHidden(4, false); // Пути project_path гарантированно открыты
        m_tableView->setColumnHidden(5, true);  // Флаг скрыт

        // ============================================================================
        // КРИТИЧЕСКИЙ ФИКС КЭША: Аппаратно заставляем Linux полностью стереть
        // графическую память таблицы и перерисовать ВСЕ ячейки по новому коду!
        // ============================================================================
        m_tableView->viewport()->update();
        m_tableView->update();

        // Отложенный ресайз шапки
        QTimer::singleShot(20, this, [this]() {
            if (m_tableView && m_tableView->horizontalHeader()) {
                m_tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
                m_tableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
                m_tableView->viewport()->update();
                m_tableView->update(); // Перестраховка для Wayland/X11
            }
        });

        if (m_lblCounter) {
            m_lblCounter->setText(QString("Всего проектов в ведомости: %1").arg(m_tableModel->rowCount()));
            m_lblCounter->setStyleSheet(QStringLiteral("color: #000000; font-weight: bold;"));
        }
    }
}

// ============================================================================
// ВСЕЯДНЫЙ РЕКУРСИВНЫЙ ПОДСЧЕТ С ВЫВОДОМ ОТЛАДКИ В КОНСОЛЬ LINUX
// ============================================================================
qint64 getDirectorySize(const QString& dirPath) {
    qint64 size = 0;
    QDir dir(dirPath);

    if (!dir.exists()) return 0;

    // Считываем абсолютно все файлы (включая скрытые файлы конфигураций и логи без расширений)
    QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoFilter);
    for (const QFileInfo& fileInfo : fileList) {
        if (fileInfo.isFile()) {
            size += fileInfo.size();
        }
    }

    // Рекурсивно спускаемся во все поддиректории
    QFileInfoList dirList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& subDirInfo : dirList) {
        size += getDirectorySize(subDirInfo.absoluteFilePath());
    }

    return size;
}

QString formatSize(qint64 bytes) {
    if (bytes <= 0) return QStringLiteral("0.0 Б");

    double size = bytes;
    QStringList units = {"Б", "Кб", "Мб", "Гб", "Тб"};
    int unitIndex = 0;

    while (size >= 1024 && unitIndex < units.size() - 1) {
        size /= 1024;
        unitIndex++;
    }

    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIndex]);
}

// ============================================================================
// УНИВЕРСАЛЬНЫЙ РЕКУРСИВНЫЙ ПОДСЧЕТ ФАЙЛОВ МОДЕЛЕЙ (PyTorch и ONNX)
// ============================================================================
int all_spisok_progects_panel::countFilesRecursive(const QString& dirPath, const QStringList& nameFilters) {
    int count = 0;
    QDir dir(dirPath);
    if (!dir.exists()) return 0;

    // Считаем файлы, подходящие под фильтр, в текущем каталоге Linux
    dir.setNameFilters(nameFilters);
    count += dir.entryInfoList(QDir::Files).size();

    // Рекурсивно заходим во все подпапки экспериментов и запусков (run_001, hf_hub и т.д.)
    dir.setNameFilters({}); // Сбрасываем фильтр для корректного поиска директорий
    QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& subDirInfo : subDirs) {
        count += countFilesRecursive(subDirInfo.absoluteFilePath(), nameFilters); // Рекурсивный вызов метода
    }

    return count;
}

void all_spisok_progects_panel::scanProjectsDirectory(const QString& targetDirPath)
{
    if (!m_tableModel) return;

    // Сначала принудительно перечитываем модель для фиксации строк
    m_tableModel->select();

    // Переключаем модель в режим ручной отправки пакета (В память)
    m_tableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    // ============================================================================
    // ЭТАП 1: ОБНОВЛЕНИЕ ТОЛЬКО МЕТАДАННЫХ В SQLite (ДЛЯ ВСЕХ СТРОК РЕЕСТРА)
    // ============================================================================
    for (int row = 0; row < m_tableModel->rowCount(); ++row) {
        QString projectPath = m_tableModel->data(m_tableModel->index(row, 4)).toString().trimmed();
        projectPath.remove(QLatin1Char('"'));
        QFileInfo projectFileInfo(projectPath);

        if (projectFileInfo.exists() && projectFileInfo.isDir()) {
            QString pureProjectName = projectFileInfo.fileName(); // Чистое имя без кружков!
            QString lastModTime = projectFileInfo.lastModified().toString("dd.MM.yyyy hh:mm");

            // Автоматически определяем регистр папок данных (data/datasets)
            QDir rootDir(projectPath);
            QString actualDataPath = "";
            QString actualDatasetsPath = "";

            QStringList entryList = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& subDirName : entryList) {
                if (subDirName.compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0) {
                    actualDataPath = rootDir.absoluteFilePath(subDirName);
                }
                else if (subDirName.compare(QStringLiteral("datasets"), Qt::CaseInsensitive) == 0) {
                    actualDatasetsPath = rootDir.absoluteFilePath(subDirName);
                }
            }

            // Вычисляем физический объем файлов обучающей выборки двигателя
            qint64 totalDatasetBytes = 0;
            if (!actualDataPath.isEmpty()) {
                totalDatasetBytes += getDirectorySize(actualDataPath);
            }
            if (!actualDatasetsPath.isEmpty()) {
                totalDatasetBytes += getDirectorySize(actualDatasetsPath);
            }
            QString currentDatasetSize = formatSize(totalDatasetBytes);

            // ЛИШНИЙ КОД ПОДСЧЕТА И ФОРМИРОВАНИЯ КРУЖКОВ/МОЗГОВ ЗДЕСЬ ПОЛНОСТЬЮ УДАЛЕН.
            // Записываем в базу данных только чистые, безопасные строки.
            m_tableModel->setData(m_tableModel->index(row, 0), pureProjectName, Qt::EditRole);
            m_tableModel->setData(m_tableModel->index(row, 1), lastModTime, Qt::EditRole);
            m_tableModel->setData(m_tableModel->index(row, 3), currentDatasetSize, Qt::EditRole);
        }
    }

    // Выталкиваем чистые изменения из памяти в SQLite
    if (!m_tableModel->submitAll()) {
        qWarning() << "[all_spisok_progects_panel] Ошибка submitAll пакета данных:" << m_tableModel->lastError().text();
    }

    // Возвращаем исходную стратегию для работы интерактивного комбобокса статусов
    m_tableModel->setEditStrategy(QSqlTableModel::OnFieldChange);

    // ============================================================================
    // ЭТАП 2: АВТОМАТИЧЕСКИЙ ПОДХВАТ НОВЫХ ПАПОК ИЗ СИСТЕМНОЙ ДИРЕКТОРИИ /projects
    // ============================================================================
    QDir projectsDir(targetDirPath);
    if (!projectsDir.exists()) return;

    projectsDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList folderList = projectsDir.entryInfoList();
    QSqlQuery checkQuery(m_db);

    for (const QFileInfo& folderInfo : folderList) {
        QString projectName = folderInfo.fileName();
        QString projectPath = folderInfo.absoluteFilePath();
        QString lastModTime = folderInfo.lastModified().toString("dd.MM.yyyy hh:mm");

        checkQuery.prepare("SELECT COUNT(*) FROM studio_projects WHERE project_path = :path");
        checkQuery.bindValue(":path", projectPath);

        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() == 0) {
            QSqlQuery insertQuery(m_db);
            insertQuery.prepare(
                "INSERT INTO studio_projects (project_name, last_modified, project_status, dataset_size, project_path, is_pinned) "
                "VALUES (:name, :mod, 'Создан', '0.0 Б', :path, 0)"
                );

            // ОЧИЩЕНО: Передаем только чистое имя папки (без принудительного добавления ⚪)
            insertQuery.bindValue(":name", projectName);
            insertQuery.bindValue(":mod", lastModTime);
            insertQuery.bindValue(":path", projectPath);
            insertQuery.exec();
        }
    }

    // Перечитываем финальный кэш для отображения изменений на экране QTableView
    m_tableModel->select();
}

void all_spisok_progects_panel::buildUiFromJson(const QJsonObject& layoutConfig)
{
    // 1. Если у виджета уже был назначен старый компоновщик, безопасно удаляем его
    if (layout()) {
        delete layout();
    }

    // 2. Если в корне JSON-конфига описан слой "layout", запускаем сборку подвиджетов
    if (layoutConfig.contains("layout")) {
        QJsonObject layoutObj = layoutConfig["layout"].toObject();

        // Вызываем функцию рекурсивного парсинга слоев и кнопок
        QLayout* rootLayout = parseLayout(layoutObj);

        if (rootLayout) {
            // КРИТИЧЕСКИЙ ШАГ ДЛЯ ОТОБРАЖЕНИЯ: Назначаем собранный слой нашему виджету
            setLayout(rootLayout);

            // Заставляем Qt6 принудительно пересчитать размеры и адаптивно
            // растянуть таблицу и кнопки на всю ширину и высоту стартового окна Linux
            rootLayout->activate();
            this->updateGeometry();

            qDebug() << "[all_spisok_progects_panel] Динамический макет успешно собран и растянут на экране.";
        }
    } else {
        qWarning() << "[all_spisok_progects_panel] Ошибка сборки: В JSON конфигурации отсутствует корневой узел 'layout'.";
    }
}

QLayout* all_spisok_progects_panel::parseLayout(const QJsonObject& layoutObj) {
    QLayout* layout = nullptr;
    QString type = layoutObj["type"].toString();

    if (type == "QVBoxLayout") layout = new QVBoxLayout();
    else if (type == "QHBoxLayout") layout = new QHBoxLayout();
    else return nullptr;

    // Парсим именованные объектные отступы, которые мы исправили в JSON
    if (layoutObj.contains("margins")) {
        QJsonObject marginsObj = layoutObj["margins"].toObject();
        layout->setContentsMargins(
            marginsObj["left"].toInt(0),
            marginsObj["top"].toInt(0),
            marginsObj["right"].toInt(0),
            marginsObj["bottom"].toInt(0)
            );
    } else {
        layout->setContentsMargins(0, 0, 0, 0);
    }

    layout->setSpacing(layoutObj["spacing"].toInt(10));

    for (QJsonValueRef childValue : layoutObj["children"].toArray()) {
        QJsonObject childObj = childValue.toObject();

        if (childObj.contains("type") && childObj["type"].toString() == "QSpacerItem") {
            if (childObj["orientation"].toString() == "Horizontal") {
                layout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
            } else {
                layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
            }
        } else {
            QWidget* childWidget = parseWidget(childObj);
            if (childWidget) layout->addWidget(childWidget);
        }
    }
    // Проходим по всем добавленным элементам слоя
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (item && item->widget()) {
            // Если этот виджет — наша таблица ведомости, отдаем ей весь приоритет растяжения (вес = 1)
            if (qobject_cast<QTableView*>(item->widget())) {
                if (auto vBox = qobject_cast<QVBoxLayout*>(layout)) {
                    vBox->setStretch(i, 1);
                } else if (auto hBox = qobject_cast<QHBoxLayout*>(layout)) {
                    hBox->setStretch(i, 1);
                }
            } else {
                // Для панелей кнопок и поиска сохраняем компактный размер (вес = 0)
                if (auto vBox = qobject_cast<QVBoxLayout*>(layout)) vBox->setStretch(i, 0);
                else if (auto hBox = qobject_cast<QHBoxLayout*>(layout)) hBox->setStretch(i, 0);
            }
        }
    }

    return layout;
}

QWidget* all_spisok_progects_panel::parseWidget(const QJsonObject& widgetObj) {
    QString widgetClass = widgetObj["widget_class"].toString();
    QString widgetName = widgetObj["widget_name"].toString();
    QWidget* widget = nullptr;

    if (widgetClass == "QWidget") {
        widget = new QWidget(this);
        if (widgetObj.contains("layout")) {
            widget->setLayout(parseLayout(widgetObj["layout"].toObject()));
        }
    }
    else if (widgetClass == "QPushButton") {
        QPushButton* btn = new QPushButton(widgetObj["text"].toString(), this);
        QString actionBinding = widgetObj["action_binding"].toString();
        connect(btn, &QPushButton::clicked, this, [this, actionBinding]() { handleAction(actionBinding); });
        widget = btn;
    }
    else if (widgetClass == "QLineEdit") {
        QLineEdit* lineEdit = new QLineEdit(this);
        lineEdit->setPlaceholderText(widgetObj["placeholder"].toString());
        if (widgetObj["action_binding"].toString() == "registry_live_search") {
            connect(lineEdit, &QLineEdit::textChanged, this, &all_spisok_progects_panel::applyLiveSearch);
        }
        widget = lineEdit;
    }
    else if (widgetClass == "QComboBox") {
        QComboBox* comboBox = new QComboBox(this);
        for (auto item : widgetObj["items"].toArray()) comboBox->addItem(item.toString());
        m_cmbStatusFilter = comboBox;
        if (widgetObj["action_binding"].toString() == "registry_status_filter_changed") {
            connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &all_spisok_progects_panel::applyStatusFilter);
        }
        widget = comboBox;
    }
    else if (widgetClass == "QLabel") {
        QLabel* label = new QLabel(widgetObj["text"].toString(), this);
        if (widgetName == "lblSelectedPathContext") m_lblPathContext = label;
        else if (widgetName == "lblCounter") m_lblCounter = label;
        widget = label;
    }
    else if (widgetClass == "QTableView") {
        QTableView* tableView = new QTableView(this);
        m_tableView = tableView;

        // КРИТИЧЕСКИЙ ШАГ: Разрешаем таблице бесконечно расширяться по вертикали и горизонтали
        tableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        // Насильно убираем любые фиксированные ограничения размеров
        tableView->setMinimumSize(0, 0);
        tableView->setMaximumSize(16777215, 16777215);

        QJsonObject props = widgetObj["properties"].toObject();
        tableView->setAlternatingRowColors(props["alternatingRowColors"].toBool(true));
        tableView->setSortingEnabled(props["sortingEnabled"].toBool(true));
        tableView->setShowGrid(props["showGrid"].toBool(true));
        tableView->setSelectionMode(QAbstractItemView::SingleSelection);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);

        if (widgetObj.contains("datasource_setup")) {
            setupTableView(tableView, widgetObj["datasource_setup"].toObject());
        }
        connect(tableView, &QTableView::clicked, this, &all_spisok_progects_panel::updateContextPath);
        widget = tableView;
    }

    if (widget) {
        widget->setObjectName(widgetName);
        if (widgetObj.contains("min_width")) widget->setMinimumWidth(widgetObj["min_width"].toInt());
        if (widgetObj.contains("max_width")) widget->setMaximumWidth(widgetObj["max_width"].toInt());
        if (widgetObj.contains("min_height")) widget->setMinimumHeight(widgetObj["min_height"].toInt());

        // ДОБАВЬТЕ ЭТУ СТРОКУ: Поддержка максимальной высоты для выравнивания шапки
        if (widgetObj.contains("max_height")) widget->setMaximumHeight(widgetObj["max_height"].toInt());

        if (widgetObj.contains("stylesheet")) widget->setStyleSheet(widgetObj["stylesheet"].toString());
    }
    return widget;
}

void all_spisok_progects_panel::setupTableView(QTableView* tableView, const QJsonObject& dataSourceObj) {
    m_tableModel = new QSqlTableModel(this, m_db);
    m_tableModel->setTable(dataSourceObj["target_table"].toString());
    m_tableModel->setEditStrategy(QSqlTableModel::OnFieldChange);
    m_tableModel->setSort(5, Qt::DescendingOrder);
    m_tableModel->select();

    // Создаем и связываем прокси-модель
    ProjectIndicatorsProxyModel* proxyModel = new ProjectIndicatorsProxyModel(this);
    proxyModel->setSourceModel(m_tableModel);

    tableView->setModel(proxyModel); // Передаем прокси-интерфейс таблице
    tableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QJsonArray columns = dataSourceObj["columns"].toArray();

    // ============================================================================
    // ИСПРАВЛЕННЫЙ ЦИКЛ: Первые колонки — под контент, последняя — растягивается
    // ============================================================================
    for (int i = 0; i < columns.size(); ++i) {
        QJsonObject colObj = columns[i].toObject();
        m_tableModel->setHeaderData(i, Qt::Horizontal, colObj["header"].toString());

        // Индекс 4 — это наш столбец project_path согласно вашему JSON-файлу
        if (i == 4) {
            // Заставляем путь занимать ВСЕ оставшееся свободное место на экране справа
            tableView->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
        } else {
            // Для названия, даты, статуса и размера оставляем строгий автоподгон по тексту
            tableView->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        }
    }

    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->horizontalHeader()->setVisible(true);
    tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->verticalHeader()->setDefaultSectionSize(36);

    // ============================================================================
    // ИНТЕГРАЦИЯ ВЫПАДАЮЩЕГО СПИСКА В ТАБЛИЦУ
    // ============================================================================
    // Назначаем выпадающий список (делегат) на колонку статусов (индекс 2)
    StatusComboBoxDelegate* statusDelegate = new StatusComboBoxDelegate(tableView);
    tableView->setItemDelegateForColumn(2, statusDelegate);

    // 1. Одиночный левый клик по названию проекта теперь ТОЛЬКО переключает звезду Избранного
    connect(tableView, &QTableView::clicked, this, [tableView](const QModelIndex& index) {
        if (index.column() == 0) {
            tableView->model()->setData(index, QVariant(), Qt::EditRole);
        }
    });

    // ============================================================================
    // ИСПРАВЛЕНИЕ: ПРАВИЛЬНЫЙ СИГНАЛ КОНТЕКСТНОГО МЕНЮ ТАБЛИЦЫ
    // ============================================================================
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tableView, &QTableView::customContextMenuRequested, this, [this, tableView](const QPoint& pos) {
        QModelIndex index = tableView->indexAt(pos);
        if (index.isValid()) {
            // Вызываем всплывающее меню, переводя локальные координаты в глобальные
            showContextMenu(index, tableView->viewport()->mapToGlobal(pos));
        }
    });

    // ============================================================================
    // ВКЛЮЧАЕМ ХОВЕР-ТРЕКИНГ МЫШИ ДЛЯ ТАБЛИЦЫ (БЕЗ ОШИБОК КОМПИЛЯЦИИ)
    // ============================================================================
    tableView->setMouseTracking(true);
    tableView->viewport()->setMouseTracking(true); // Включаем трекинг для внутренней сетки

    // Подключаем сигнал движения курсора над ячейками таблицы
    connect(tableView, &QTableView::entered, this, [tableView](const QModelIndex& index) {
                // Если курсор зашел в первую колонку (Название проекта, где стоит звезда)
                if (index.column() == 0) {
                    int columnWidth = tableView->columnWidth(0);

                    // ПРАВИЛЬНЫЙ НАТИВНЫЙ ПЕРЕВОД КООРДИНАТ КУРСОРA ВНУТРЬ ЯЧЕЙКИ В Qt6
                    QPoint localMousePos = tableView->mapFromGlobal(QCursor::pos());
                    QRect cellRect = tableView->visualRect(index);
                    int localX = localMousePos.x() - cellRect.left();

                    // Звёздочка всегда отрисовывается в самом левом краю ячейки (первые 20% её ширины)
                    if (localX >= 0 && localX < (columnWidth * 0.20)) {
                        // Если курсор строго над звездой — меняем стрелку на интерактивную руку (указатель ссылки)
                        tableView->setCursor(Qt::PointingHandCursor);
                        tableView->setToolTip(QString::fromUtf8("Нажмите, чтобы закрепить проект в топе"));
                    } else {
                        // Если ведем мышь дальше вправо по имени проекта — возвращаем дефолтную стрелку
                        tableView->setCursor(Qt::ArrowCursor);
                        tableView->setToolTip(QString()); // Возвращаем штатный ToolTip паспорта двигателя
                    }
                } else {
                    // Во всех остальных столбцах (дата, статус, мегабайты, пути) — строгая стандартная стрелка
                    tableView->setCursor(Qt::ArrowCursor);
                    tableView->setToolTip(QString());
                }
            });
}

void all_spisok_progects_panel::handleAction(const QString& actionId) {
    if (actionId == "registry_db_reload") {
        scanProjectsDirectory(QStringLiteral("/home/elf/pyTorch-Studio/projects"));
        if (m_tableModel) {
            m_tableModel->select();

            if (m_tableView) {
                m_tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
                m_tableView->verticalHeader()->setDefaultSectionSize(36);
                for (int i = 0; i < m_tableModel->rowCount(); ++i) {
                    m_tableView->setRowHeight(i, 36);
                }
                m_tableView->setColumnHidden(5, true);
                m_tableView->setColumnHidden(4, false);

                // Отложенный перерасчет ширины ячеек при ручном обновлении данных
                // Отложенный перерасчет ширины ячеек при ручном обновлении данных
                QTimer::singleShot(20, this, [this]() {
                    if (m_tableView && m_tableView->horizontalHeader()) {
                        m_tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

                        // Возвращаем режим Stretch для 4-й колонки пути при обновлении
                        m_tableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
                    }
                });

            }
        }
    }
    else if (actionId == "registry_export_csv") {
        if (!m_tableModel) return;
        QString fileName = QFileDialog::getSaveFileName(this, "Экспорт ведомости проектов", "", "CSV файлы (*.csv)");
        if (fileName.isEmpty()) return;

        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << "Project Name,Last Modified,Status,Dataset Size\n";
            for (int r = 0; r < m_tableModel->rowCount(); ++r) {
                stream << m_tableModel->data(m_tableModel->index(r, 0)).toString() << ","
                       << m_tableModel->data(m_tableModel->index(r, 1)).toString() << ","
                       << m_tableModel->data(m_tableModel->index(r, 2)).toString() << ","
                       << m_tableModel->data(m_tableModel->index(r, 3)).toString() << "\n";
            }
            file.close();
        }
    }
    else if (actionId == "studio_load_selected_project") {
        if (!m_tableView || !m_tableModel) return;
        QModelIndex current = m_tableView->currentIndex();
        if (current.isValid()) {
            QString pName = m_tableModel->data(m_tableModel->index(current.row(), 0)).toString();
            emit requestOpenProject(pName);
        }
    }
    else if (actionId == "studio_change_status_popup") {
        if (!m_tableView || !m_tableModel) return;
        QModelIndex current = m_tableView->currentIndex();
        if (current.isValid()) {
            QString pName = m_tableModel->data(m_tableModel->index(current.row(), 0)).toString();
            emit requestChangeStatus(pName);
        }
    }
}

void all_spisok_progects_panel::applyLiveSearch(const QString& text) {
    if (!m_tableModel) return;
    if (text.isEmpty()) {
        m_tableModel->setFilter("");
    } else {
        m_tableModel->setFilter(QString("project_name LIKE '%%1%'").arg(text));
    }
    m_tableModel->select();
}

void all_spisok_progects_panel::applyStatusFilter(int index) {
    if (!m_tableModel || !m_cmbStatusFilter) return;
    QString selectedStatus = m_cmbStatusFilter->itemText(index);
    if (selectedStatus == "Все статусы") {
        m_tableModel->setFilter("");
    } else {
        m_tableModel->setFilter(QString("project_status = '%1'").arg(selectedStatus));
    }
    m_tableModel->select();
}
void all_spisok_progects_panel::updateContextPath() {
    if (!m_tableView || !m_tableModel || !m_lblPathContext) return;
    QModelIndex current = m_tableView->currentIndex();
    if (current.isValid()) {
        QString projectName = m_tableModel->data(m_tableModel->index(current.row(), 0)).toString();
        QString fullPath = QString("/home/elf/pyTorch-Studio/src/%1").arg(projectName);
        m_lblPathContext->setText(QString("ℹ️ Путь к выбранному проекту: %1").arg(fullPath));
    }
}

void all_spisok_progects_panel::showContextMenu(const QModelIndex& proxyIndex, const QPoint& globalPos)
{
    if (!m_tableModel || !m_tableView) return;

    // Получаем реальный индекс строки в оригинальной QSqlTableModel
    int sourceRow = proxyIndex.row();

    // Извлекаем чистые данные из SQLite по этой строке
    QString pureName = m_tableModel->data(m_tableModel->index(sourceRow, 0)).toString();
    QString projectPath = m_tableModel->data(m_tableModel->index(sourceRow, 4)).toString().trimmed();
    projectPath.remove(QLatin1Char('"'));

    // Создаем стилизованное меню IDE
    QMenu contextMenu(this);
    contextMenu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: #ffffff; border: 1px solid #cbd5e1; padding: 4px; border-radius: 4px; }"
        "QMenu::item { padding: 6px 28px 6px 10px; border-radius: 2px; color: #1e293b; }"
        "QMenu::item:selected { background-color: #3b82f6; color: #ffffff; }"
        "QMenu::separator { height: 1px; background: #e2e8f0; margin: 4px 0; }"
        ));

    // --- ГРУППА 1: Управление ядром Студии ---
    QAction* actLoad = contextMenu.addAction(QString::fromUtf8("🚀 Загрузить мотор в рабочую область"));
    QAction* actPin = contextMenu.addAction(QString::fromUtf8("📌 Закрепить / Открепить в топе"));

    contextMenu.addSeparator();

    // --- ГРУППА 2: Интеграция с ОС Linux ---
    QAction* actDolphin = contextMenu.addAction(QString::fromUtf8("📁 Открыть в проводнике Dolphin"));
    QAction* actTerminal = contextMenu.addAction(QString::fromUtf8("💻 Открыть терминал в папке"));

    contextMenu.addSeparator();

    // --- ГРУППА 3: Обслуживание ИИ-моделей ---
    QAction* actClean = contextMenu.addAction(QString::fromUtf8("🧹 Очистить промежуточный кэш обучения"));
    QAction* actRecalc = contextMenu.addAction(QString::fromUtf8("🔄 Принудительно пересчитать параметры"));

    contextMenu.addSeparator();

    // --- ГРУППА 4: Удаление и безопасность ---
    QAction* actRemoveList = contextMenu.addAction(QString::fromUtf8("❌ Удалить только из ведомости"));
    QAction* actDeleteDisk = contextMenu.addAction(QString::fromUtf8("🗑️ Безвозвратно стереть с диска"));

    // Запускаем меню у курсора и ждем выбора инженера
    QAction* selected = contextMenu.exec(globalPos);
    if (!selected) return;

    // ============================================================================
    // ОБРАБОТЧИКИ ВЫБРАННЫХ ДЕЙСТВИЙ ИНЖЕНЕРА
    // ============================================================================
    if (selected == actLoad) {
        emit requestOpenProject(pureName);
    }
    else if (selected == actPin) {
        // Триггерим метод setData нашего прокси-интерфейса для инверсии звезды
        m_tableView->model()->setData(proxyIndex, QVariant(), Qt::EditRole);
    }
    else if (selected == actDolphin) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(projectPath));
    }
    else if (selected == actTerminal) {
        // Вызываем нативный терминал KDE/Breeze Linux (Konsole) прямо в рабочей папке мотора
        QStringList arguments;
        arguments << QStringLiteral("--workdir") << projectPath;
        QProcess::startDetached(QStringLiteral("konsole"), arguments);
    }
    else if (selected == actClean) {
        // Логика очистки: заходим в models/ и удаляем все промежуточные файлы, кроме лучшего
        QDir modelsDir(projectPath + QStringLiteral("/models"));
        if (modelsDir.exists()) {
            QStringList filters = {"*.pt", "*.pth"};
            QFileInfoList list = modelsDir.entryInfoList(filters, QDir::Files);
            for (const QFileInfo& f : list) {
                if (f.fileName() != "best.pt" && f.fileName() != "best.pth") {
                    QFile::remove(f.absoluteFilePath());
                }
            }
            refreshPanel(); // Перечитываем и обновляем счетчики на экране
            QMessageBox::information(this, QString::fromUtf8("PyTorch Studio"), QString::fromUtf8("Промежуточные чекпоинты весов успешно удалены. Оставлен только best.pt."));
        }
    }
    else if (selected == actRecalc) {
        refreshPanel();
    }
    else if (selected == actRemoveList) {
        // Удаляем строчку строго из реестра SQLite studio_global.db
        QSqlQuery delQuery(m_db);
        delQuery.prepare("DELETE FROM studio_projects WHERE project_path = :path");
        delQuery.bindValue(":path", projectPath);
        if (delQuery.exec()) {
            m_tableModel->select(); // Обновляем сетку таблицы на экране
        }
    }
    else if (selected == actDeleteDisk) {
        // Выводим строгое предупреждение безопасности Linux
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            QString::fromUtf8("Критическое удаление"),
            QString::fromUtf8("Вы уверены, что хотите полностью стереть папку проекта и все ИИ-модели с жесткого диска?\nПуть: %1").arg(projectPath),
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            QDir dirToDelete(projectPath);
            if (dirToDelete.exists() && dirToDelete.removeRecursively()) {
                // Если с диска папка удалена успешно — зачищаем и SQLite строку реестра
                QSqlQuery delQuery(m_db);
                delQuery.prepare("DELETE FROM studio_projects WHERE project_path = :path");
                delQuery.bindValue(":path", projectPath);
                delQuery.exec();
                m_tableModel->select();
            } else {
                QMessageBox::warning(this, QString::fromUtf8("Ошибка"), QString::fromUtf8("Не удалось удалить папку. Проверьте права доступа в Linux."));
            }
        }
    }
}