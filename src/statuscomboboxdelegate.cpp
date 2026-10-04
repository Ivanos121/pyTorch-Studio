#include "statuscomboboxdelegate.h"
#include <QComboBox>

StatusComboBoxDelegate::StatusComboBoxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget* StatusComboBoxDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/*option*/, const QModelIndex &/*index*/) const
{
    QComboBox *editor = new QComboBox(parent);

    // Список этапов (строго соответствует элементам фильтра cmbStatusFilter из JSON)
    editor->addItems({
        QStringLiteral("Создан"),
        QStringLiteral("В разработке"),
        QStringLiteral("Обучение модели"),
        QStringLiteral("Прошит в STM"),
        QStringLiteral("Завершено")
    });

    return editor;
}

void StatusComboBoxDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QComboBox *comboBox = qobject_cast<QComboBox*>(editor);
    if (comboBox) {
        QString currentStatus = index.model()->data(index, Qt::EditRole).toString();
        int comboIndex = comboBox->findText(currentStatus);
        if (comboIndex != -1) {
            comboBox->setCurrentIndex(comboIndex);
        }
    }
}

void StatusComboBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QComboBox *comboBox = qobject_cast<QComboBox*>(editor);
    if (comboBox) {
        model->setData(index, comboBox->currentText(), Qt::EditRole);
    }
}

void StatusComboBoxDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/*index*/) const
{
    editor->setGeometry(option.rect);
}
