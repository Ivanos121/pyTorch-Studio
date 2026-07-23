#include "qhtmldelegate.h"
#include <QPainter>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QStyle>
#include <QFontMetrics>
#include <QPainterPath>

QHtmlDelegate::QHtmlDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void QHtmlDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true); // Включаем сглаживание для углов ячеек!

    QColor darkBg(35, 38, 41);     // #232629
    QColor selectBg(26, 74, 110);   // #1a4a6e

    // =========================================================================
    // БРОНИРОВАННЫЙ UX-ПРОБОЙ ВЕРХНИХ УГЛОВ: СКРУГЛЕНИЕ ВНУТРИ ДЕЛЕГАТА
    // =========================================================================
    if (option.state & QStyle::State_Selected) {
        // Если это САМАЯ ПЕРВАЯ строка списка (индекс 0), скругляем ей только верхние углы!
        if (index.row() == 0) {
            painter->save();
            QPainterPath path;
            // Рисуем path со скруглением верхних углов (радиус 7px), низ оставляем прямой (0px)
            path.addRoundedRect(option.rect, 7, 7, Qt::AbsoluteSize);

            // Срезаем нижние углы, чтобы они оставались прямоугольными и не было щелей со 2-й строкой
            QPainterPath clipBottom;
            clipBottom.addRect(option.rect.left(), option.rect.top() + 7, option.rect.width(), option.rect.height() - 7);
            path = path.united(clipBottom);

            painter->setClipPath(path);
            painter->fillRect(option.rect, selectBg);
            painter->restore();
        } else {
            // Для всех остальных строк выделение остается обычным прямоугольным
            painter->fillRect(option.rect, selectBg);
        }
    } else {
        // Точно так же обрабатываем пассивное состояние для первой строки
        if (index.row() == 0) {
            painter->save();
            QPainterPath path;
            path.addRoundedRect(option.rect, 7, 7, Qt::AbsoluteSize);
            QPainterPath clipBottom;
            clipBottom.addRect(option.rect.left(), option.rect.top() + 7, option.rect.width(), option.rect.height() - 7);
            path = path.united(clipBottom);

            painter->setClipPath(path);
            painter->fillRect(option.rect, darkBg);
            painter->restore();
        } else {
            painter->fillRect(option.rect, darkBg);
        }
    }
    // =========================================================================

    // ОРИГИНАЛЬНЫЙ КОД ОТРИСОВКИ ТЕКСТА (ОСТАЕТСЯ БЕЗ ИЗМЕНЕНИЙ)
    QString rawHtml = index.data(Qt::DisplayRole).toString();
    QString cleanText = rawHtml;
    cleanText.remove(QRegularExpression("<[^>]*>"));

    QFont font = option.font;
    font.setFamily("JetBrains Mono");
    font.setPixelSize(12);
    painter->setFont(font);

    QTextLayout layout(cleanText, font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    layout.endLayout();

    int textHeight = painter->fontMetrics().height();
    int yOffset = option.rect.top() + (option.rect.height() - textHeight) / 2 - 1;

    int prefixLength = 0;
    QRegularExpression prefixRegex("<b>(.*?)</b>");
    QRegularExpressionMatch match = prefixRegex.match(rawHtml);
    if (match.hasMatch()) {
        prefixLength = match.captured(1).length();
    }

    QColor blueColor(76, 195, 255);
    QColor whiteColor(239, 240, 241);

    int startX = option.rect.left() + 10;
    if (prefixLength > 0 && prefixLength <= cleanText.length()) {
        QString typedPart = cleanText.left(prefixLength);
        painter->setPen(blueColor);
        QFont boldFont = font;
        boldFont.setBold(true);
        painter->setFont(boldFont);
        painter->drawText(startX, yOffset, painter->fontMetrics().horizontalAdvance(typedPart), option.rect.height(), Qt::AlignLeft, typedPart);
        startX += painter->fontMetrics().horizontalAdvance(typedPart);
    }

    QString restPart = cleanText.mid(prefixLength);
    painter->setPen(whiteColor);
    painter->setFont(font);
    painter->drawText(startX, yOffset, painter->fontMetrics().horizontalAdvance(restPart) + 50, option.rect.height(), Qt::AlignLeft, restPart);

    painter->restore();
}







