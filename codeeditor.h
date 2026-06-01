// codeeditor.h
#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QWidget>
#include <QPainter>
#include <QRect>
#include <QSize>

class CodeEditor;

// Класс для отрисовки вертикальной линейки номеров
class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor *editor);
    QSize sizeHint() const override { return QSize(0, 0); } // Заглушка, реальный размер берется из CodeEditor
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    CodeEditor *codeEditor;
};

// Основной класс продвинутого текстового редактора с номерами строк
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    CodeEditor(QWidget *parent = nullptr);
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
};

#endif // CODEEDITOR_H
