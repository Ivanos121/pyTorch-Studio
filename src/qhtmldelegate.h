#ifndef HTMLDELEGATE_H
#define HTMLDELEGATE_H

#include <QStyledItemDelegate>

class QHtmlDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit QHtmlDelegate(QObject *parent = nullptr);

    // Переопределяем метод отрисовки строки списка
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // HTMLDELEGATE_H
