#ifndef STICKYSCROLLAREA_H
#define STICKYSCROLLAREA_H

#include <QWidget>
#include <QTextBlock>
#include <QVector>

class CodeEditor;

class StickyScrollArea : public QWidget
{
    Q_OBJECT
public:
    explicit StickyScrollArea(CodeEditor *editor);

    // Метод для ручного пересчета геометрии при resizeEvent редактора
    void updateGeometrySize();

public slots:
    void calculateStickyBlocks();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;


private:
    CodeEditor *m_editor;
    QVector<QTextBlock> m_stickyBlocks;
    int m_lineHeight;
};

#endif // STICKYSCROLLAREA_H
