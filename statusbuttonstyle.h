#ifndef STATUSBUTTONSTYLE_H
#define STATUSBUTTONSTYLE_H

#include <QProxyStyle>
#include <QStyleOptionButton>
#include <QPainter>

class StatusButtonStyle : public QProxyStyle {
    Q_OBJECT

public:
    explicit StatusButtonStyle(QStyle *baseStyle = nullptr);

    // Меняем drawPrimitive на drawControl
    void drawControl(ControlElement element,
                     const QStyleOption *option,
                     QPainter *painter,
                     const QWidget *widget = nullptr) const override;
};

#endif // STATUSBUTTONSTYLE_H
