#include "variablestablehandler.h"

VariablesTableHandler::VariablesTableHandler(QTreeView *treeView, QObject *parent)
    : QObject(parent), m_treeView(treeView)
{
    if (!m_treeView) return;

    // Создаем модель данных
    m_model = new QStandardItemModel(this);
    m_model->setColumnCount(ColCount);
    m_model->setHorizontalHeaderLabels({"Имя", "Тип", "Значение"});

    // Привязываем модель к TreeView
    m_treeView->setModel(m_model);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers); // Запрет редактирования
    m_treeView->setAnimated(true);                                 // Плавное раскрытие веток

    // Настройка размеров колонок
    // QHeaderView *header = m_treeView->horizontalHeader();
    // header->setSectionResizeMode(ColName, QHeaderView::Stretch);
    // header->setSectionResizeMode(ColType, QHeaderView::ResizeToContents);
    // header->setSectionResizeMode(ColValue, QHeaderView::Stretch);
}

void VariablesTableHandler::clear() {
    if (m_model) m_model->removeRows(0, m_model->rowCount());
}

void VariablesTableHandler::updateVariables(const QList<VariableData> &variables) {
    if (!m_model) return;

    clear(); // Очищаем старые переменные перед обновлением

    for (const auto &var : variables) {
        // Создаем элементы для первой строки дерева (корневые переменные)
        QStandardItem *itemName  = new QStandardItem(var.name);
        QStandardItem *itemType  = new QStandardItem(var.type);
        QStandardItem *itemValue = new QStandardItem(var.value);

        // Добавляем строку в корень модели
        int nextRow = m_model->rowCount();
        m_model->setItem(nextRow, ColName, itemName);
        m_model->setItem(nextRow, ColType, itemType);
        m_model->setItem(nextRow, ColValue, itemValue);

        // Если у переменной есть вложенные элементы, добавляем их рекурсивно
        if (var.hasChildren) {
            for (const auto &child : var.children) {
                addVariableNode(itemName, child);
            }
        }
    }
}

void VariablesTableHandler::addVariableNode(QStandardItem *parentItem, const VariableData &var) {
    if (!parentItem) return;

    QStandardItem *childName  = new QStandardItem(var.name);
    QStandardItem *childType  = new QStandardItem(var.type);
    QStandardItem *childValue = new QStandardItem(var.value);

    // Добавляем как вложенную строку к parentItem
    QList<QStandardItem*> row;
    row << childName << childType << childValue;
    parentItem->appendRow(row);

    // Если есть более глубокая вложенность (например, dict внутри dict)
    if (var.hasChildren) {
        for (const auto &child : var.children) {
            addVariableNode(childName, child);
        }
    }
}
