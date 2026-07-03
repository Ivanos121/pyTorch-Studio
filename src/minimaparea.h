#pragma once

#include <QWidget>
#include <QPoint>

class CodeEditor;

class MinimapArea : public QWidget {
    Q_OBJECT
public:
    explicit MinimapArea(CodeEditor *editor, QWidget *parent = nullptr);
    void handleMouseMoveFromEditor(const QPoint &pos);
    void handleMouseLeaveFromEditor();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    CodeEditor *m_editor;
    QPoint m_mousePos;
    bool m_showLens;
};
