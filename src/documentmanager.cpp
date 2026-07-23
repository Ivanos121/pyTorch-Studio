#include "documentmanager.h"
#include "neuro_programm.h"
#include <QProcess>
#include <QDebug>
#include <QStackedWidget>

DocumentManager::DocumentManager(QMainWindow *mainWindow, QComboBox *fileCombo, QListWidget *openFilesList, QLabel *titleLbl, QObject *parent)
    : QObject(parent), m_window(mainWindow), m_fileCombo(fileCombo), m_filesListWidget(openFilesList), m_titleLabel(titleLbl)
{
    // =========================================================================
    // СИНХРОНИЗАЦИЯ СТАРТОВЫХ ИМЕН СЕРВИСНЫХ СТРАНИЦ
    // =========================================================================
    if (m_filesListWidget && m_fileCombo) {
        m_filesListWidget->clear();
        m_fileCombo->clear();

        // 1. Вкладка "Панель управления"
        m_openedFiles.append("MAIN_SCREEN");
        QListWidgetItem *mainItem = new QListWidgetItem(m_filesListWidget);
        mainItem->setText(" Панель управления");
        mainItem->setData(Qt::UserRole, QString("MAIN_SCREEN"));
        mainItem->setIcon(QIcon(":/Data/system_icons/configure.svg"));
        m_filesListWidget->addItem(mainItem);
        m_fileCombo->addItem(" Панель управления", QString("MAIN_SCREEN"));

        // 2. Вкладка "AI-ассистент"
        m_openedFiles.append("AI_CHAT_SCREEN");
        QListWidgetItem *chatItem = new QListWidgetItem(m_filesListWidget);
        chatItem->setText(" AI-ассистент");
        chatItem->setData(Qt::UserRole, QString("AI_CHAT_SCREEN"));
        chatItem->setIcon(QIcon(":/Data/system_icons/edit-find.svg"));
        m_filesListWidget->addItem(chatItem);
        m_fileCombo->addItem(" AI-ассистент", QString("AI_CHAT_SCREEN"));

        // =====================================================================
        // ЖЕСТКИЙ АППАРАТНЫЙ UX-ФИКС: ОПРЕДЕЛЯЕМ СТИЛЬ ПОДСВЕТКИ ВЫДЕЛЕНИЯ
        // =====================================================================
        m_filesListWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #ffffff;"
            "   border: none;"
            "   outline: 0;" /* Полностью убирает пунктирную рамку фокуса */
            "}"
            "QListWidget::item {"
            "   padding-top: 4px;"
            "   padding-bottom: 4px;"
            "   color: #232629;"
            "   border-radius: 4px;"
            "}"
            "QListWidget::item:hover {"
            "   background-color: #e4e5e6;"
            "}"
            "QListWidget::item:selected {"
            "   background-color: #93cee9 !important;"
            "   color: #000000 !important;"
            "   font-weight: bold;"
            "}"
            // ИСПРАВЛЕНО: Заменен некорректный !active на валидный :!active
            "QListWidget::item:selected:!active {"
            "   background-color: #93cee9 !important;"
            "   color: #000000 !important;"
            "   font-weight: bold;"
            "}"
            );

        m_activeFilePath = "MAIN_SCREEN";
        m_filesListWidget->setCurrentRow(0); // Сразу подсвечиваем синим первую строку
    }
}

QString DocumentManager::getCleanProjectName() const
{
    if (!m_window) return QString();
    QString projectPath = m_window->property("currentOpenProjectPath").toString().trimmed();

    if (projectPath.isEmpty() && !m_activeFilePath.isEmpty()) {
        QFileInfo info(m_activeFilePath);
        QDir d(info.absolutePath());
        d.cdUp();
        return d.dirName();
    }
    if (projectPath.isEmpty()) return QString();
    return QDir(projectPath).dirName();
}
void DocumentManager::registerNewOpenFile(const QString &absoluteFilePath, CodeEditor *editor)
{
    if (absoluteFilePath.isEmpty() || m_openedFiles.contains(absoluteFilePath)) return;
    m_openedFiles.append(absoluteFilePath);
    QFileInfo info(absoluteFilePath);

    // 1. АППАРАТНО СОЗДАЕМ ЯЧЕЙКУ В ЛЕВОМ СПИСКЕ "ОТКРЫТЫЕ ДОКУМЕНТЫ"
    QListWidgetItem *item = new QListWidgetItem(m_filesListWidget);
    item->setText(" " + info.fileName());
    item->setData(Qt::UserRole, absoluteFilePath);
    item->setIcon(QIcon(":/Data/system_icons/text-x-python.svg"));
    m_filesListWidget->addItem(item);

    // 2. ДОБАВЛЯЕМ ЭЛЕМЕНТ В ЦЕНТРАЛЬНЫЙ ВЕРХНИЙ КОМБОБОКС (передаем полный путь как userData)
    if (m_fileCombo) {
        m_fileCombo->addItem(info.fileName(), absoluteFilePath);
    }

    // 3. ПОДПИСЫВАЕМСЯ НА ФЛАГ ИЗМЕНЕНИЯ ТЕКСТА РЕДАКТОРА (ДЛЯ ЗВЕЗДОЧЕК)
    if (editor && editor->document()) {
        connect(editor->document(), &QTextDocument::modificationChanged, this, [this, absoluteFilePath](bool changed) {
            this->handleDocumentModificationChanged(absoluteFilePath, changed);
        });
    }

    // Активируем синхронизацию комбобокса и перерисовку шапки
    handleFileActivation(absoluteFilePath);
}

void DocumentManager::handleFileActivation(const QString &absoluteFilePath)
{
    m_activeFilePath = absoluteFilePath.trimmed();

    // 1. Синхронизация верхнего комбобокса
    if (m_fileCombo) {
        int comboIdx = m_fileCombo->findData(m_activeFilePath);
        if (comboIdx != -1) {
            m_fileCombo->blockSignals(true);
            m_fileCombo->setCurrentIndex(comboIdx);
            m_fileCombo->blockSignals(false);
        }
    }

    // =========================================================================
    // ЖЕЛЕЗНЫЙ UX-ФИКС: ПОИСК СТРОКИ И ПРИНУДИТЕЛЬНОЕ ОКРАШИВАНИЕ ЧЕРЕЗ setCurrentRow
    // =========================================================================
    if (m_filesListWidget) {
        for (int i = 0; i < m_filesListWidget->count(); ++i) {
            QListWidgetItem *item = m_filesListWidget->item(i);
            if (!item) continue;

            // Сравниваем очищенные ключи метаданных
            if (item->data(Qt::UserRole).toString().trimmed() == m_activeFilePath) {
                m_filesListWidget->blockSignals(true);

                // ИСПРАВЛЕНО: Задаем индекс строки! Это нативно зажжет синий цвет Qt6
                m_filesListWidget->setCurrentRow(i);
                item->setSelected(true);
                m_filesListWidget->scrollToItem(item);

                m_filesListWidget->blockSignals(false);
                break;
            }
        }
        m_filesListWidget->update(); // Форсируем моментальный рендер
    }

    // Обновляем строку заголовка главного окна ОС Linux
    updateUiTitles(m_activeFilePath);
}

void DocumentManager::handleDocumentModificationChanged(const QString &absoluteFilePath, bool isModified)
{
    QFileInfo info(absoluteFilePath);
    QString baseName = info.fileName();

    if (absoluteFilePath == "MAIN_SCREEN" || absoluteFilePath == "AI_CHAT_SCREEN") return;

    QString displayName = isModified ? baseName + " *" : baseName;

    // 1. УМНОЕ ОБНОВЛЕНИЕ ВЕРХНЕГО КОМБОБОКСА СО ЗВЕЗДОЧКОЙ
    if (m_fileCombo) {
        int comboIdx = m_fileCombo->findData(absoluteFilePath); // Теперь совпадение 100% найдется!
        if (comboIdx != -1) {
            m_fileCombo->setItemText(comboIdx, displayName);
            m_fileCombo->update();
        }
    }

    // 2. ОБНОВЛЕНИЕ ЛЕВОГО СПИСКА "ОТКРЫТЫЕ ДОКУМЕНТЫ"
    if (m_filesListWidget) {
        for (int i = 0; i < m_filesListWidget->count(); ++i) {
            QListWidgetItem *item = m_filesListWidget->item(i);
            if (item && item->data(Qt::UserRole).toString() == absoluteFilePath) {
                item->setText(isModified ? " " : " " + baseName);
                if (isModified) {
                    item->setText(" " + baseName + " *");
                }
                m_filesListWidget->update();
                break;
            }
        }
    }

    if (m_activeFilePath == absoluteFilePath) {
        updateUiTitles(absoluteFilePath);
    }
}

void DocumentManager::handleFileClosed(const QString &absoluteFilePath)
{
    m_openedFiles.removeAll(absoluteFilePath);

    int comboIdx = m_fileCombo ? m_fileCombo->findData(absoluteFilePath) : -1;
    if (comboIdx != -1) m_fileCombo->removeItem(comboIdx);

    if (m_filesListWidget) {
        for (int i = 0; i < m_filesListWidget->count(); ++i) {
            QListWidgetItem *item = m_filesListWidget->item(i);
            if (item && item->data(Qt::UserRole).toString() == absoluteFilePath) {
                delete m_filesListWidget->takeItem(i);
                break;
            }
        }
    }

    if (m_activeFilePath == absoluteFilePath) {
        m_activeFilePath = m_openedFiles.isEmpty() ? "" : m_openedFiles.last();
        handleFileActivation(m_activeFilePath);
    }
}
void DocumentManager::updateUiTitles(const QString &absoluteFilePath)
{
    QString realFullPath = absoluteFilePath.trimmed();
    QString projectPath = m_window ? m_window->property("currentOpenProjectPath").toString().trimmed() : "";

    // =========================================================================
    // СЦЕНАРИЙ А: ПРОЕКТ ПОЛНОСТЬЮ ЗАКРЫТ ИЛИ ЕЩЕ НЕ ВЫБРАН (ЧИСТЫЙ ХОЛСТ)
    // =========================================================================
    if (projectPath.isEmpty() || projectPath.contains("build") || projectPath.contains("Debug")) {
        m_window->setWindowTitle("PyTorch Studio");
        QLabel *liveTitleLabel = m_window->findChild<QLabel*>("titleLabel");
        if (liveTitleLabel) liveTitleLabel->setText("PyTorch Studio");
        return;
    }

    QString projName = QDir(projectPath).dirName();
    QLabel *liveTitleLabel = m_window->findChild<QLabel*>("titleLabel");

    // =========================================================================
    // СЦЕНАРИЙ Б: ОБРАБОТКА СЕРВИСНЫХ СТРАНИЦ ИЛИ ПУСТОГО ПРОЕКТА (БЕЗ ФАЙЛОВ!)
    // =========================================================================
    if (realFullPath.isEmpty() || realFullPath == "MAIN_SCREEN" || realFullPath == "AI_CHAT_SCREEN")
    {
        if (realFullPath == "MAIN_SCREEN") {
            QString title = QString("Панель управления[%1.pystudio] - PyTorch Studio").arg(projName);
            m_window->setWindowTitle(title);
            if (liveTitleLabel) liveTitleLabel->setText(title);
        }
        else if (realFullPath == "AI_CHAT_SCREEN") {
            QString title = QString("AI-ассистент[%1.pystudio] - PyTorch Studio").arg(projName);
            m_window->setWindowTitle(title);
            if (liveTitleLabel) liveTitleLabel->setText(title);
        }
        else {
            QString title = QString("[%1.pystudio] - PyTorch Studio").arg(projName);
            m_window->setWindowTitle(title);
            if (liveTitleLabel) liveTitleLabel->setText(title);
        }
        return;
    }

    // =========================================================================
    // СЦЕНАРИЙ В: ИДЕТ АКТИВНАЯ РАБОТА С РЕАЛЬНЫМ ФАЙЛОМ КОДА
    // =========================================================================
    if (!realFullPath.contains("/")) {
        QStackedWidget *centralStack = m_window->findChild<QStackedWidget*>("centralStackedWidget");
        QWidget *currentPage = centralStack ? centralStack->currentWidget() : nullptr;
        if (currentPage && currentPage->objectName().contains("/")) {
            realFullPath = currentPage->objectName();
        }
    }

    QFileInfo fileInfo(realFullPath);
    QString fileName = fileInfo.fileName();

    QString modifiedMarker = "";
    int comboIdx = m_fileCombo ? m_fileCombo->findData(realFullPath) : -1;
    if (comboIdx != -1 && m_fileCombo->itemText(comboIdx).endsWith(" *")) {
        modifiedMarker = "*";
    }

    QString absoluteFileDir = QDir::cleanPath(fileInfo.absolutePath());
    QStringList pathSegments = absoluteFileDir.split("/", Qt::SkipEmptyParts);
    QString relativeSubDir = "";

    int projectIdx = pathSegments.indexOf(projName);
    if (projectIdx != -1 && projectIdx < pathSegments.size() - 1) {
        QStringList subDirSegments;
        for (int i = projectIdx + 1; i < pathSegments.size(); ++i) {
            QString segment = pathSegments[i].trimmed();
            if (segment.contains("build") || segment.contains("Debug") || segment.contains("qt6"))
                continue;
            subDirSegments.append(segment);
        }
        relativeSubDir = subDirSegments.join("/");
    }

    if (relativeSubDir.isEmpty() || relativeSubDir == ".") {
        if (absoluteFileDir.toLower().contains("/scripts")) relativeSubDir = "scripts";
        else if (absoluteFileDir.toLower().contains("/config")) relativeSubDir = "config";
        else relativeSubDir = projName;
    }

    QString finalTitle = QString("%1%2(%3@%4)[%4.pystudio] - PyTorch Studio")
                             .arg(fileName).arg(modifiedMarker).arg(relativeSubDir).arg(projName);

    m_window->setWindowTitle(finalTitle);
    if (liveTitleLabel) {
        liveTitleLabel->setText(finalTitle);
        liveTitleLabel->update();
    }
}
