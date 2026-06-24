#include "aiprojectmodel.h"

AIProjectModel::AIProjectModel(QObject *parent)
    : QAbstractItemModel(parent), m_sourceModel(nullptr)
{
}

void AIProjectModel::setSourceModel(QFileSystemModel *sourceModel, const QString &rootPath)
{
    beginResetModel();
    m_sourceModel = sourceModel;
    m_rootPath = QFileInfo(rootPath).absoluteFilePath();
    m_projectName = QDir(rootPath).dirName();
    m_pystudioPath = QFileInfo(m_rootPath + "/" + m_projectName + ".pystudio").absoluteFilePath();
    endResetModel();
}

int AIProjectModel::columnCount(const QModelIndex &) const {
    return 1; // Отображаем только одну колонку "Имя"
}

QModelIndex AIProjectModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_sourceModel || column != 0) return QModelIndex();

    // 1. Уровень 0: Самый верхний корень дерева (Папка проекта)
    if (!parent.isValid()) {
        if (row == 0) return createIndex(row, column, (quintptr)1);
        return QModelIndex();
    }

    quintptr parentId = parent.internalId();

    // 2. Уровень 1: Внутри папки проекта (id == 1) лежит ТОЛЬКО узел .pystudio
    if (parentId == 1) {
        if (row == 0) return createIndex(row, column, (quintptr)2);
        return QModelIndex();
    }

    // 3. Уровень 2: Внутри узла .pystudio (id == 2) отображаем содержимое папки проекта
    if (parentId == 2) {
        QModelIndex sourceRoot = m_sourceModel->index(m_rootPath);
        int validRow = 0;
        for (int i = 0; i < m_sourceModel->rowCount(sourceRoot); ++i) {
            QModelIndex srcIdx = m_sourceModel->index(i, 0, sourceRoot);
            if (QFileInfo(m_sourceModel->filePath(srcIdx)).absoluteFilePath() == m_pystudioPath) continue; // Пропускаем сам файл паспорта

            if (validRow == row) {
                // Магия Qt: упаковываем реальный внутренний указатель дисковой модели
                return createIndex(row, column, srcIdx.internalPointer());
            }
            validRow++;
        }
        return QModelIndex();
    }

    // 4. Уровень 3 и глубже: Элементы внутри подпапок (datasets, models и т.д.)
    // Родителем здесь выступает реальный физический элемент на диске
    QModelIndex sourceParent = mapToSource(parent);
    if (sourceParent.isValid()) {
        QModelIndex sourceChild = m_sourceModel->index(row, 0, sourceParent);
        if (sourceChild.isValid()) {
            return createIndex(row, column, sourceChild.internalPointer());
        }
    }

    return QModelIndex();
}

QModelIndex AIProjectModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || !m_sourceModel) return QModelIndex();
    quintptr id = child.internalId();

    if (id == 1) return QModelIndex(); // У корня проекта нет родителя
    if (id == 2) return createIndex(0, 0, (quintptr)1); // Родителем .pystudio является папка проекта

    // Для всех глубоких вложенных файлов восстанавливаем родителя через исходную модель файловой системы
    QModelIndex sourceIndex = mapToSource(child);
    if (sourceIndex.isValid()) {
        QModelIndex sourceParent = sourceIndex.parent();

        // Если родитель этого элемента — это корень проекта, значит его виртуальный родитель — узел .pystudio (id: 2)
        if (QFileInfo(m_sourceModel->filePath(sourceParent)).absoluteFilePath() == m_rootPath) {
            return createIndex(0, 0, (quintptr)2);
        }

        // Если это глубокий подкаталог, ищем его родительскую строку на диске
        if (sourceParent.isValid()) {
            return createIndex(sourceParent.row(), 0, sourceParent.internalPointer());
        }
    }

    return QModelIndex();
}

int AIProjectModel::rowCount(const QModelIndex &parent) const
{
    if (!m_sourceModel) return 0;

    if (!parent.isValid()) return 1; // Корень проекта = 1 строка
    quintptr id = parent.internalId();

    if (id == 1) return 1; // Внутри корня проекта только .pystudio = 1 строка
    if (id == 2) {
        // Количество элементов на первом уровне (минус сам файл .pystudio)
        QModelIndex sourceRoot = m_sourceModel->index(m_rootPath);
        int total = m_sourceModel->rowCount(sourceRoot);
        return total > 0 ? total - 1 : 0;
    }

    // Для всех подпапок рекурсивно запрашиваем количество файлов у QFileSystemModel
    QModelIndex sourceIndex = mapToSource(parent);
    if (sourceIndex.isValid() && m_sourceModel->isDir(sourceIndex)) {
        return m_sourceModel->rowCount(sourceIndex);
    }

    return 0;
}

QVariant AIProjectModel::data(const QModelIndex &proxyIndex, int role) const
{
    if (!proxyIndex.isValid() || !m_sourceModel) return QVariant();
    quintptr id = proxyIndex.internalId();

    if (role == Qt::DisplayRole) {
        if (id == 1) return m_projectName;
        if (id == 2) return QString("%1 [.pystudio]").arg(m_projectName);

        // Для всех остальных узлов делегируем отображение текста базовой модели диска
        QModelIndex sourceIndex = mapToSource(proxyIndex);
        return m_sourceModel->data(sourceIndex, role);
    }

    if (role == Qt::DecorationRole) {
        // Системные иконки Qt для папок проекта
        if (id == 1) return QApplication::style()->standardIcon(QStyle::SP_DirIcon);
        if (id == 2) return QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView); // Иконка паспорта проекта

        QModelIndex sourceIndex = mapToSource(proxyIndex);
        return m_sourceModel->data(sourceIndex, role);
    }

    return QVariant();
}

QModelIndex AIProjectModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || !m_sourceModel) return QModelIndex();
    quintptr id = proxyIndex.internalId();

    if (id == 1) return m_sourceModel->index(m_rootPath);
    if (id == 2) return m_sourceModel->index(m_pystudioPath);

    // Восстанавливаем оригинальный QModelIndex диска из сохраненного internalPointer
    return m_sourceModel->index(proxyIndex.row(), 0, m_sourceModel->index(m_rootPath));
}

QModelIndex AIProjectModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid() || !m_sourceModel) return QModelIndex();
    QString srcPath = QFileInfo(m_sourceModel->filePath(sourceIndex)).absoluteFilePath();

    if (srcPath == m_rootPath) return createIndex(0, 0, (quintptr)1);
    if (srcPath == m_pystudioPath) return createIndex(0, 0, (quintptr)2);

    // Для внутренних элементов вычисляем их строку относительно родителя
    return createIndex(sourceIndex.row(), 0, sourceIndex.internalPointer());
}
