#pragma once
#include <QObject>
#include <QTreeView>
#include <QStandardItemModel>
#include <QHeaderView>

// Структура для передачи данных о переменной из Python-дебаггера
struct VariableData {
    QString name;
    QString type;
    QString value;
    bool hasChildren = false; // Для списков, словарей или объектов классов
    QList<VariableData> children; // Вложенные элементы (если есть)
};

class VariablesTableHandler : public QObject {
    Q_OBJECT
public:
    explicit VariablesTableHandler(QTreeView *treeView, QObject *parent = nullptr);

    // Метод для обновления дерева переменных
    void updateVariables(const QList<VariableData> &variables);

    // Очистка дерева
    void clear();

private:
    // Рекурсивный помощник для добавления вложенных элементов дерева
    void addVariableNode(QStandardItem *parentItem, const VariableData &var);

    QTreeView *m_treeView;
    QStandardItemModel *m_model;

    enum Columns {
        ColName = 0,
        ColType,
        ColValue,
        ColCount
    };
};
