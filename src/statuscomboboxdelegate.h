#pragma once

#include <QStyledItemDelegate>

class StatusComboBoxDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit StatusComboBoxDelegate(QObject *parent = nullptr);
    ~StatusComboBoxDelegate() override = default;

    // Создание комбобокса при старте редактирования ячейки
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Перенос текущих данных из ячейки таблицы в открывшийся комбобокс
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    // Сохранение выбранного в комбобоксе значения обратно в модель и SQLite
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    // Подгонка геометрических размеров комбобокса под рамки ячейки таблицы
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
