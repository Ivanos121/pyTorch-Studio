#ifndef BREEZEFLATSTYLE_H
#define BREEZEFLATSTYLE_H

#include <QProxyStyle>
#include <QStyleOptionComboBox>
#include <QStyleOptionSpinBox>
#include <QPainter>

class BreezeFlatStyle : public QProxyStyle {
    Q_OBJECT

public:
    explicit BreezeFlatStyle(QStyle *baseStyle = nullptr) : QProxyStyle(baseStyle) {}

    // Метод перехвата отрисовки сложных элементов управления (стрелок и кнопок)
    void drawComplexControl(ComplexControl control,
                            const QStyleOptionComplex *option,
                            QPainter *painter,
                            const QWidget *widget = nullptr) const override;
};

#endif // BREEZEFLATSTYLE_H
