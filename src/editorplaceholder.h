#ifndef EDITORPLACEHOLDER_H
#define EDITORPLACEHOLDER_H

#include <QWidget>
#include <QPainter>
#include <QPair>
#include <QList>

class EditorPlaceholder : public QWidget {
    Q_OBJECT
public:
    explicit EditorPlaceholder(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // EDITORPLACEHOLDER_H
