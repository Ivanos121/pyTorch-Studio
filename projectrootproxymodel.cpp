#include "projectrootproxymodel.h"

ProjectRootProxyModel::ProjectRootProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void ProjectRootProxyModel::setProjectInfo(const QString &rootPath, const QString &projectName)
{
    m_rootPath = QDir(rootPath).absolutePath();
    m_projectName = projectName;
}

QVariant ProjectRootProxyModel::data(const QModelIndex &proxyIndex, int role) const
{
    QModelIndex sourceIndex = mapToSource(proxyIndex);

    if (role == Qt::DisplayRole && sourceIndex.isValid())
    {
        QFileSystemModel *fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
        if (fsModel)
        {
            QString currentPath = fsModel->filePath(sourceIndex);

            // Сравниваем абсолютные пути к файлам через каноничные строки QFileInfo
            if (!currentPath.isEmpty() &&
                QFileInfo(currentPath).absoluteFilePath() == m_rootPath)
            {
                // Если это корень — возвращаем красивое имя а-ля Qt Creator
                return QString("%1 [%2.pystudio]").arg(m_projectName, m_projectName);
            }
        }
    }
    return QSortFilterProxyModel::data(proxyIndex, role);
}

bool ProjectRootProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QFileSystemModel *fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
    if (!fsModel) return true;

    // Получаем индекс и полный путь к элементу, который Qt собирается отрисовать в дереве
    QModelIndex childIndex = fsModel->index(source_row, 0, source_parent);
    QString childPath = QFileInfo(fsModel->filePath(childIndex)).absoluteFilePath();
    QString currentRoot = QFileInfo(m_rootPath).absoluteFilePath();

    // Проверяем, находится ли элемент на самом верхнем уровне (в родительской папке ОС)
    if (fsModel->filePath(source_parent) == fsModel->rootPath())
    {
        // ПРАВИЛО: На самом верху разрешаем показывать ТОЛЬКО нашу папку проекта,
        // все остальные соседние каталоги жестко скрываем для безопасности UI
        return (childPath == currentRoot);
    }

    // ДЛЯ ВСЕХ ВНУТРЕННИХ УРОВНЕЙ ПРОЕКТА:
    // Разрешаем отображать абсолютно всё без исключений. Файл z1.pystudio НЕ СКРЫВАЕТСЯ.
    return true;
}


