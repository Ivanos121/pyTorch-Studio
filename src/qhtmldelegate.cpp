#include "qhtmldelegate.h"
#include <QPainter>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QStyle>

QHtmlDelegate::QHtmlDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void QHtmlDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem options = option;
    initStyleOption(&options, index);

    painter->save();

    QTextDocument doc;
    doc.setDefaultFont(options.font);

    // =========================================================================
    // ЖЕЛЕЗНЫЙ UX ФИКС: Принудительный глобальный CSS-стиль для QTextDocument
    // =========================================================================
    // Задаем базовый цвет (color: #eff0f1) для ВСЕГО текста по умолчанию.
    // Теперь любой текст, не обернутый в синий span, гарантированно станет светлым!
    doc.setDefaultStyleSheet("body, span, p { color: #eff0f1; }");

    // Передаем HTML-строку (Qt применит наш стиль к тегам <span>)
    doc.setHtml(options.text);
    // =========================================================================

    options.text = ""; // Очищаем дефолтный текст
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter, options.widget);

    // Вычисляем идеальный вертикальный отступ, чтобы текст не обрезался снизу
    int textHeight = doc.size().height();
    int topOffset = options.rect.top() + (options.rect.height() - textHeight) / 2;

    // Сдвигаем по Х на 6 пикселей от края, а по Y выставляем строго по центру строки
    painter->translate(options.rect.left() + 6, topOffset);

    QRectF clip(0, 0, options.rect.width() - 6, options.rect.height());
    doc.drawContents(painter, clip);

    painter->restore();
}


