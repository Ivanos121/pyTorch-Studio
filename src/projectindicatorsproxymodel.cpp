#include "projectindicatorsproxymodel.h"
#include "all_spisok_progects_panel.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlTableModel>
#include <QDebug>
#include <QDesktopServices>

qint64 getDirectorySize(const QString& dirPath);

ProjectIndicatorsProxyModel::ProjectIndicatorsProxyModel(QObject *parent)
    : QIdentityProxyModel(parent)
{
}

QVariant ProjectIndicatorsProxyModel::data(const QModelIndex &proxyIndex, int role) const
{
    // ============================================================================
    // РЕАЛИЗАЦИЯ ПУНКТА 1: ВСПЛЫВАЮЩИЙ ПАСПОРТ ДВИГАТЕЛЯ (TOOLTIP)
    // ============================================================================
    if (role == Qt::ToolTipRole && proxyIndex.column() == 0) {
        QString projectPath = sourceModel()->data(sourceModel()->index(proxyIndex.row(), 4)).toString().trimmed();
        projectPath.remove(QLatin1Char('"'));

        // Формируем красивую HTML-карточку паспорта с техническими параметрами мотора
        QString tooltipText = QStringLiteral(
            "<div style='font-family: monospace; font-size: 12px; line-height: 1.4; color: #1e293b;'>"
            "  <b style='color: #2563eb; font-size: 13px;'>📋 ПАСПОРТ ДВИГАТЕЛЯ</b><br>"
            "  <hr style='border: none; border-top: 1px solid #e2e8f0; margin: 4px 0;'>"
            );

        // Будущая интеграция с Задача №2: Пытаемся прочитать данные из project_local.db или json
        QString localDbPath = projectPath + QStringLiteral("/db/project_local.db");
        QFileInfo dbCheck(localDbPath);

        if (dbCheck.exists()) {
            // Если база констант из Задачи №2 уже создана, выводим реальные физические параметры
            tooltipText += QStringLiteral("  Status: <span style='color: #16a34a; font-weight: bold;'>Локальная БД развернута</span><br>");
            tooltipText += QStringLiteral("  Путь к БД: %1<br>").arg(localDbPath);
        } else {
            // Если проект пустой, выводим базовые ожидаемые параметры железа
            tooltipText += QStringLiteral("  Активное сопротивление (R): <span style='color: #dc2626;'>Не задано</span><br>");
            tooltipText += QStringLiteral("  Индуктивность фазы (L): <span style='color: #dc2626;'>Не задано</span><br>");
            tooltipText += QStringLiteral("  Пары полюсов ротора (p): <span style='color: #dc2626;'>Не задано</span><br>");
        }

        tooltipText += QStringLiteral(
            "  <hr style='border: none; border-top: 1px solid #e2e8f0; margin: 4px 0;'>"
            "  <span style='color: #64748b; font-size: 10px;'>💡 Кликните дважды, чтобы загрузить мотор в ядро Студии</span>"
            "</div>"
            );
        return tooltipText;
    }

    // 1. НАСТРОЙКА ВИЗУАЛЬНОГО ОТОБРАЖЕНИЯ ЗВЕЗДОЧЕК НА ЭКРАНЕ
    if (role == Qt::DisplayRole && proxyIndex.column() == 0) {
        QString projectPath = sourceModel()->data(sourceModel()->index(proxyIndex.row(), 4)).toString().trimmed();
        projectPath.remove(QLatin1Char('"'));
        QFileInfo projectFileInfo(projectPath);

        if (projectFileInfo.exists() && projectFileInfo.isDir()) {
            QString pureName = projectFileInfo.fileName();

            // Вытаскиваем текущий статус закрепления из 5-й колонки SQLite (is_pinned)
            int isPinned = sourceModel()->data(sourceModel()->index(proxyIndex.row(), 5)).toInt();

            // Считаем файлы телеметрии (Контур 1)
            int dataFilesCount = 0;
            QStringList targetDataFolders = { QStringLiteral("data"), QStringLiteral("datasets") };
            for (const QString& folderName : targetDataFolders) {
                QString subPath = projectPath + QLatin1Char('/') + folderName;
                if (QDir(subPath).exists()) {
                    dataFilesCount += all_spisok_progects_panel::countFilesRecursive(subPath, {QStringLiteral("*.*")});
                }
            }

            // Рекурсивно считаем веса PyTorch (Контур 2) и ONNX (Контур 3)
            int ptCount = 0;
            QString modelsPath = projectPath + QStringLiteral("/models");
            if (QDir(modelsPath).exists()) {
                ptCount = all_spisok_progects_panel::countFilesRecursive(modelsPath, {QStringLiteral("*.pt"), QStringLiteral("*.pth")});
            }

            int onnxCount = 0;
            QDir rootDir(projectPath);
            QString actualHubPath = "";
            QStringList folderEntries = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& subName : folderEntries) {
                if (subName.compare(QStringLiteral("hf_hub"), Qt::CaseInsensitive) == 0 ||
                    subName.compare(QStringLiteral("hf_h"), Qt::CaseInsensitive) == 0) {
                    actualHubPath = rootDir.absoluteFilePath(subName);
                    break;
                }
            }
            if (!actualHubPath.isEmpty() && QDir(actualHubPath).exists()) {
                onnxCount = all_spisok_progects_panel::countFilesRecursive(actualHubPath, {QStringLiteral("*.onnx")});
            }

            // Собираем префикс индикаторов ИИ
            QString prefix = "";

            // САМЫЙ ПЕРВЫЙ СИМВОЛ: Интерактивная звезда Избранного
            if (isPinned == 1) {
                prefix += QString::fromUtf8("⭐ "); // Закрашенная звезда для закрепленных проектов
            } else {
                prefix += QString::fromUtf8("☆ "); // Контурная звезда для обычных проектов
            }

            // Добавляем наши три контура диагностики ИИ
            if (dataFilesCount > 0) prefix += QString::fromUtf8("🟢") + QString("(%1) ").arg(dataFilesCount);
            else                       prefix += QString::fromUtf8("⚪(0) ");

            if (ptCount > 0)   prefix += QString::fromUtf8("🔵") + QString("(%1) ").arg(ptCount);
            else               prefix += QString::fromUtf8("⚪(0) ");

            if (onnxCount > 0) prefix += QString::fromUtf8("🟣") + QString("(%1) ").arg(onnxCount);
            else               prefix += QString::fromUtf8("⚪(0) ");

            return prefix + pureName;
        }
    }

    return QIdentityProxyModel::data(proxyIndex, role);
}

QVariant ProjectIndicatorsProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && section == 0 && role == Qt::DisplayRole) {
        return QStringLiteral("Название проекта");
    }
    return QIdentityProxyModel::headerData(section, orientation, role);
}

// 2. ИНТЕРАКТИВНЫЙ ПЕРЕХВАТ КЛИКОВ МЫШИ ДЛЯ ИЗМЕНЕНИЯ СТАТУСА ИЗБРАННОГО
bool ProjectIndicatorsProxyModel::setData(const QModelIndex &proxyIndex, const QVariant &value, int role)
{
    // Объединяем проверки первой колонки и роли редактирования в один главный защитный контур
    if (proxyIndex.column() == 0 && (role == Qt::EditRole || role == Qt::CheckStateRole)) {
        QSqlTableModel* sqlModel = qobject_cast<QSqlTableModel*>(sourceModel());
        if (sqlModel) {

            // ВЕТКА А: Перехват клика по крайней правой части (Вызов Dolphin)
            if (value.toString() == QStringLiteral("CLICKED_DOLPHIN")) {
                QString projectPath = sqlModel->data(sqlModel->index(proxyIndex.row(), 4)).toString().trimmed();
                projectPath.remove(QLatin1Char('"'));

                // Открываем нативный менеджер Linux (Dolphin / Nautilus)
                QDesktopServices::openUrl(QUrl::fromLocalFile(projectPath));
                qDebug() << "[Quick Actions] Нативный менеджер Linux Dolphin успешно открыт для пути:" << projectPath;

                return true; // Прерываем выполнение, звезда НЕ переключится!
            }

            // ВЕТКА Б: Клик пришелся по левой части — штатно переключаем Избранное (Pinning)
            else {
                // Вытаскиваем текущее значение из 5-й колонки базы данных (is_pinned)
                int currentPinned = sqlModel->data(sqlModel->index(proxyIndex.row(), 5)).toInt();

                // Инвертируем статус: если было 0, станет 1, и наоборот
                int newPinned = (currentPinned == 1) ? 0 : 1;

                // Записываем новый статус напрямую в SQLite-модель
                sqlModel->setData(sqlModel->index(proxyIndex.row(), 5), newPinned, Qt::EditRole);
                sqlModel->submitAll(); // Толкаем трансляцию изменений на SSD
                sqlModel->select();    // Заставляем таблицу перегруппироваться (Пин мгновенно улетит вверх!)

                emit dataChanged(proxyIndex, proxyIndex); // Даем команду Qt6 принудительно обновить экран
                return true;
            }
        }
    }

    // Для всех остальных колонок или ролей вызываем дефолтный базовый метод Qt
    return QIdentityProxyModel::setData(proxyIndex, value, role);
}

