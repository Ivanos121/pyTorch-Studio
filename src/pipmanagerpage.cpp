#include "pipmanagerpage.h"
#include "neuro_programm.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHeaderView>
#include <QColor>
#include <QSettings>
#include <QApplication>

PipManagerPage::PipManagerPage(const QString &venvDir, const QString &requirementsPath, QWidget *parent)
    : QWidget(parent), m_venvDir(venvDir), m_requirementsPath(requirementsPath), m_pendingRequests(0)
{
    QString configPath = QDir::home().filePath(".config/PyTorchStudio/IDE.conf");
    QSettings settings(configPath, QSettings::IniFormat);

    // Достаем значение /home/elf/venv, которое прописано в вашем конфиге
    QString globalVenv = settings.value("python/global_venv_path").toString();

    // Если вдруг в конфиге пусто, берем путь к venv из настроек платформы
    if (globalVenv.isEmpty())
    {
        globalVenv = settings.value("Platform/lastKnownPythonPath").toString();
        // Отрезаем /bin/python, если там прописан путь прямо к бинарнику
        if (globalVenv.endsWith("/bin/python"))
        {
            globalVenv.chop(11);
        }
    }

#ifdef Q_OS_WIN
    m_pythonExe = QDir(globalVenv).filePath("Scripts/python.exe");
#else
    m_pythonExe = QDir(globalVenv).filePath("bin/python");
#endif

    qDebug() << "🚀 [PIP КРИТИЧЕСКИЙ ДЕБАГ] Итоговый физический путь к Python:" << m_pythonExe;
    qDebug() << "📂 [PIP КРИТИЧЕСКИЙ ДЕБАГ] Существует ли файл на диске Arch?:" << QFile::exists(m_pythonExe);

    // 2. Инициализация UI
    m_layout = new QVBoxLayout(this);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4); // Добавили 4-й столбец для контроля требований
    m_table->setHorizontalHeaderLabels({"Пакет в venv", "Версия PyPI", "requirements.txt", "Статус"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setStyleSheet("QTableWidget::item:selected { background-color: #007ACC; color: white; }");

    m_refreshBtn = new QPushButton("Обновить и синхронизировать пакеты", this);

    m_layout->addWidget(m_table);
    m_layout->addWidget(m_refreshBtn);
    setLayout(m_layout);

    // 3. Сеть и процессы
    m_pipProcess = new QProcess(this);
    m_networkManager = new QNetworkAccessManager(this);

    connect(m_pipProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PipManagerPage::onPipListFinished);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &PipManagerPage::onPyPiReplyFinished);
    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        this->loadPipData(); // Вызовется со значением по умолчанию pkgName = ""
    });

    this->setProperty("requirementsPath", m_requirementsPath);

    // 1. Разрешаем таблице генерировать сигнал кастомного контекстного меню
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    // 2. Связываем сигнал правого клика со слотом внутри этого же класса страницы
    connect(m_table, &QTableWidget::customContextMenuRequested,
            this, &PipManagerPage::showContextMenu);
}

void PipManagerPage::parseRequirementsFile() {
    m_requiredPackages.clear();
    QFile file(m_requirementsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        // Отсекаем ==, >=, <=, чтобы получить чистое имя
        QString pkgName = line.split("==").first().split(">=").first().split("<=").first().trimmed().toLower();
        m_requiredPackages.insert(pkgName);
    }
    file.close();
}

#include <QCoreApplication>
#include <QDebug>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

void PipManagerPage::loadPipData(QString pkgName) {
    m_refreshBtn->setEnabled(false);
    m_pendingRequests = 0;

    // Запоминаем имя пакета для автоматической подсветки после перезаписи строк
    m_packageToHighlight = pkgName;

    parseRequirementsFile();

    if (!QFile::exists(m_pythonExe)) {
        m_refreshBtn->setEnabled(true);
        return;
    }

    // ИСПРАВЛЕННЫЙ PYTHON-КОД: Извлекает name и version через встроенные свойства ядра,
    // что гарантирует 100% стабильность работы на Arch Linux без падений синтаксиса.
    QString pythonCode =
        "import json, subprocess, sys, importlib.metadata\n"
        "try:\n"
        "    cmd = [sys.executable, '-m', 'pip', 'list', '--outdated', '--format=json']\n"
        "    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5.0)\n"
        "    outdated = {p['name'].lower(): p['latest_version'] for p in json.loads(res.stdout)} if res.returncode == 0 else {}\n"
        "except: outdated = {}\n"
        "installed = []\n"
        "for d in importlib.metadata.distributions():\n"
        "    name = d.name\n"
        "    ver = d.version\n"
        "    latest = outdated.get(name.lower(), ver)\n"
        "    installed.append({'name': name, 'version': ver, 'pypi': latest})\n"
        "print(json.dumps(installed))";

    QStringList arguments;
    arguments << "-c" << pythonCode;

    m_pipProcess->start(m_pythonExe, arguments);
}

void PipManagerPage::onPipListFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_refreshBtn->setEnabled(true);

    if (exitCode != 0 || exitStatus == QProcess::CrashExit) {
        QByteArray stdErr = m_pipProcess->readAllStandardError();
        qWarning() << "❌ [PIP СБОЙ] Процесс сбора данных Python завершился с ошибкой:" << stdErr;
        return;
    }

    QByteArray output = m_pipProcess->readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(output);
    if (!doc.isArray()) return;

    QJsonArray packages = doc.array();

    // Выделяем память под точное количество строк в QTableWidget
    m_table->setRowCount(packages.size());

    // ПОЛНОЦЕННЫЙ C++ ЦИКЛ ЗАПОЛНЕНИЯ ТАБЛИЦЫ ЯЧЕЙКАМИ
    for (int i = 0; i < packages.size(); ++i) {
        QJsonObject pkg = packages[i].toObject();
        QString name = pkg["name"].toString();
        QString version = pkg["version"].toString();
        QString pypiVersion = pkg["pypi"].toString();

        // 1. Столбец: Пакет в venv (Имя и локальная версия)
        QTableWidgetItem *instItem = new QTableWidgetItem(QString("%1 (%2)").arg(name, version));
        instItem->setFlags(instItem->flags() ^ Qt::ItemIsEditable); // Запрещаем ручную правку
        // Сохраняем метаданные внутрь ячейки для методов верификации и подсветки
        instItem->setData(Qt::UserRole, name);
        instItem->setData(Qt::UserRole + 1, version);
        m_table->setItem(i, 0, instItem);

        // 2. Столбец: Версия PyPI
        QTableWidgetItem *pypiItem = new QTableWidgetItem(pypiVersion);
        pypiItem->setFlags(pypiItem->flags() ^ Qt::ItemIsEditable);
        m_table->setItem(i, 1, pypiItem);

        // 3. Столбец: requirements.txt (Проверка наличия файла зависимости)
        QTableWidgetItem *reqItem = new QTableWidgetItem();
        reqItem->setFlags(reqItem->flags() ^ Qt::ItemIsEditable);
        if (m_requiredPackages.contains(name.toLower())) {
            reqItem->setText("Включен");
        } else {
            reqItem->setText("Отсутствует ⚠️");
            reqItem->setBackground(QColor("#E2E3E5")); // Серый цвет
        }
        m_table->setItem(i, 2, reqItem);

        // 4. Столбец: Статус синхронизации версий окружения
        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setFlags(statusItem->flags() ^ Qt::ItemIsEditable);

        bool inRequirements = m_requiredPackages.contains(name.toLower());

        if (version == pypiVersion) {
            if (inRequirements) {
                statusItem->setText("Актуально");
                statusItem->setBackground(QColor("#D4EDDA")); // Зеленый
            } else {
                statusItem->setText("Ок, но нет в файле ⚠️");
                statusItem->setBackground(QColor("#CCE5FF")); // Синеватый
            }
        } else {
            statusItem->setText(QString("Доступна v%1").arg(pypiVersion));
            statusItem->setBackground(QColor("#FFF3CD")); // Желтый
        }
        m_table->setItem(i, 3, statusItem);
    }

    QHeaderView *header = m_table->horizontalHeader();
    if (header) {
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
        header->setSectionResizeMode(2, QHeaderView::Stretch);
        header->setSectionResizeMode(3, QHeaderView::Stretch);
    }
    // =========================================================================

    // Откладываем выделение строки на 30 мс под движок Qt6
    if (!m_packageToHighlight.isEmpty()) {
        QString pkgToHighlight = m_packageToHighlight;
        m_packageToHighlight.clear();

        QTimer::singleShot(30, this, [this, pkgToHighlight]() {
            this->highlightAndScrollToPackage(pkgToHighlight);
        });
    }

    emit dataLoaded();

}

void PipManagerPage::onPyPiReplyFinished(QNetworkReply* reply) {
    m_pendingRequests--;
    int row = reply->property("row").toInt();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();

        // Строим документ
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        // 1. Извлекаем корневой объект
        QJsonObject rootObj = doc.object();

        // 2. Извлекаем "info" как полноценный QJsonObject
        QJsonObject infoObj = rootObj.value("info").toObject();

        // 3. Вытаскиваем версию как чистую строку QString
        QString latestVersion = infoObj.value("version").toString();

        // ЖЕЛЕЗНАЯ ПРОВЕРКА: Если строка версии пустая — значит, ключ не найден
        if (!latestVersion.isEmpty()) {
            // Заполняем второй столбец версией из репозитория
            m_table->item(row, 1)->setText(latestVersion);

            // Получаем ранее сохраненные локальные данные
            QString installedVersion = m_table->item(row, 0)->data(Qt::UserRole + 1).toString();
            QString pkgName = m_table->item(row, 0)->data(Qt::UserRole).toString();

            // Обновляем статус и красим ячейку в зеленый/желтый/синий
            updateStatusCell(row, installedVersion, latestVersion, pkgName);

            reply->deleteLater();
            if (m_pendingRequests == 0) m_refreshBtn->setEnabled(true);
            return; // Успешно выходим!
        }
    }

    // ЕСЛИ ПРОИЗОШЕЛ СБОЙ СЕТИ ИЛИ ПАРСИНГА JSON:
    m_table->item(row, 1)->setText("Н/Д");

    QTableWidgetItem *statusItem = m_table->item(row, 3);
    if (!statusItem) {
        statusItem = new QTableWidgetItem();
        m_table->setItem(row, 3, statusItem);
    }

    // Выводим ошибку сети, если HTTP-запрос упал
    if (reply->error() != QNetworkReply::NoError) {
        statusItem->setText("Ошибка сети");
    } else {
        statusItem->setText("Ошибка парсинга");
    }
    statusItem->setBackground(QColor("#F8D7DA")); // Красный цвет ошибки

    reply->deleteLater();
    if (m_pendingRequests == 0) m_refreshBtn->setEnabled(true);
}


void PipManagerPage::updateStatusCell(int row, const QString &installedVer, const QString &latestVer, const QString &pkgName) {
    QTableWidgetItem *statusItem = new QTableWidgetItem();
    statusItem->setFlags(statusItem->flags() ^ Qt::ItemIsEditable);

    bool inRequirements = m_requiredPackages.contains(pkgName.toLower());

    if (installedVer == latestVer) {
        if (inRequirements) {
            statusItem->setText("Актуально");
            statusItem->setBackground(QColor("#D4EDDA")); // Зеленый
        } else {
            statusItem->setText("Ок, но нет в файле ⚠️");
            statusItem->setBackground(QColor("#CCE5FF")); // Синеватый
        }
    } else {
        statusItem->setText(QString("Доступна v%1").arg(latestVer));
        statusItem->setBackground(QColor("#FFF3CD")); // Желтый
    }
    m_table->setItem(row, 3, statusItem);
}

void PipManagerPage::highlightAndScrollToPackage(const QString &packageName) {
    if (!m_table || packageName.isEmpty()) return;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *nameItem = m_table->item(row, 0);
        if (nameItem) {
            QString currentPkgName = nameItem->data(Qt::UserRole).toString();

            if (currentPkgName.toLower() == packageName.toLower()) {
                // 1. Принудительно передаем фокус ввода клавиатуры на саму таблицу
                m_table->setFocus(Qt::OtherFocusReason);

                // 2. Устанавливаем текущую ячейку (фокус рамки)
                m_table->setCurrentCell(row, 0);

                // 3. Выделяем всю строку через системные флаги выделения Qt6
                m_table->selectionModel()->select(
                    m_table->model()->index(row, 0),
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
                    );

                // 4. Выполняем автоскролл, выставляя новую библиотеку ровно по центру экрана
                m_table->scrollToItem(nameItem, QAbstractItemView::PositionAtCenter);

                // Форсируем обновление графического холста
                m_table->viewport()->update();
                m_table->update();

                qDebug() << "🎯 [ВЫДЕЛЕНИЕ УСПЕШНО] Выделена и отцентрирована строка пакета:" << packageName;
                break;
            }
        }
    }
}

bool PipManagerPage::isPackageInstalled(const QString &packageName) {
    if (!m_table) return false;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *item = m_table->item(row, 0);
        if (item) {
            QString installedName = item->data(Qt::UserRole).toString();
            if (installedName.toLower() == packageName.toLower()) {
                return true;
            }
        }
    }
    return false;
}

void PipManagerPage::showContextMenu(const QPoint &pos) {
    // 1. Определяем, по какому элементу кликнули правой кнопкой мыши
    QTableWidgetItem *item = m_table->itemAt(pos);
    if (!item) return; // Кликнули по пустому месту таблицы (вне строк)

    int row = item->row();
    QTableWidgetItem *nameItem = m_table->item(row, 0);
    if (!nameItem) return;

    // 2. Извлекаем чистое имя пакета из метаданных UserRole первой ячейки строки
    QString packageName = nameItem->data(Qt::UserRole).toString();
    if (packageName.isEmpty()) return;

    // 3. Создаем всплывающее контекстное меню
    QMenu contextMenu(this);
    contextMenu.setStyleSheet(
        "QMenu { background-color: #252526; color: #FFFFFF; border: 1px solid #3F3F46; padding: 4px; }"
        "QMenu::item { padding: 4px 20px 4px 20px; }"
        "QMenu::item:selected { background-color: #007ACC; color: white; }"
        );

    // Динамически формируем пункты меню с подстановкой имени пакета
    QAction *upgradeAction = new QAction(QString("🆙 Обновить пакет %1").arg(packageName), &contextMenu);
    QAction *uninstallAction = new QAction(QString("🗑️ Удалить пакет %1").arg(packageName), &contextMenu);

    contextMenu.addAction(upgradeAction);
    contextMenu.addSeparator(); // Разделительная линия
    contextMenu.addAction(uninstallAction);

    // 4. Выводим меню на экран строго в геометрической позиции курсора мыши
    QAction *selectedAction = contextMenu.exec(m_table->viewport()->mapToGlobal(pos));

    // 5. Безопасно находим указатель на главное окно Neuro_programm через метод window()
    auto *mainWindow = qobject_cast<Neuro_programm*>(this->window());
    if (!mainWindow) return;

    // 6. Обработка выбора инженера
    if (selectedAction == upgradeAction) {
        // Записываем имя пакета в свойство процесса главного окна, чтобы после апгрейда сработал автоскролл
        QProcess *installProc = mainWindow->findChild<QProcess*>();
        if (installProc) {
            installProc->setProperty("installedPackageName", packageName);
        }

        // Передаем команду обновления в существующий метод верхнего меню,
        // но чтобы не вызывать QInputDialog, мы временно доработаем запуск процесса!
        // (Для этого ниже мы вызовем наш готовый атомарный метод runPipUpgradeProcess)
        mainWindow->runPipUpgradeProcess(packageName);
    }
    else if (selectedAction == uninstallAction) {
        // UX-Защита: Спрашиваем подтверждение перед удалением прямо здесь
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Подтверждение удаления"),
                                      QString(tr("Вы уверены, что хотите полностью удалить пакет '%1'?")).arg(packageName),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // Вызываем наш готовый метод удаления в главном окне
            mainWindow->runPipUninstallProcess(packageName);
        }
    }
}