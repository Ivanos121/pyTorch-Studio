// codeeditor.h
#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QWidget>
#include <QPainter>
#include <QRect>
#include <QSize>
#include <QCompleter>
#include <QListWidget>
#include <QPaintEvent>
#include <QTextBlockUserData>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>

class CodeEditor;
class Neuro_program;
class FoldingArea;

class FolderBlockData : public QTextBlockUserData
{
public:
    int indentLevel = 0;        // Уровень отступа строки
    bool isFoldStart = false;   // Начало блока (def/class)
    bool isFolded = false;      // Свернут ли блок
};

class PythonHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    PythonHighlighter(QTextDocument *parent = nullptr);
    void loadThemeSettings();
    QTextCharFormat keywordFormat;
    QTextCharFormat constantFormat;
    QTextCharFormat pytorchFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat multiLineCommentFormat;

protected:
    // Главный метод, который Qt автоматически вызывает для каждой видимой строки
    void highlightBlock(const QString &text) override;

private:
    // Структура для хранения правила: регулярное выражение + формат цвета
    QHash<QRegularExpression, QTextCharFormat*> highlightingRulesMap;
    QRegularExpression tripleSingleQuote;
    QRegularExpression tripleDoubleQuote;
};

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
    friend class Neuro_programm;
    friend class FoldingArea;

public:
    CodeEditor(QWidget *parent = nullptr);
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void setCompleter(QCompleter *completer);
    QString textUnderCursor() const;
    QStringList temporaryOpenFilesBackup;
    void updateFoldingData();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *e) override;
    void foldingAreaPaintEvent(QPaintEvent *event);
    void foldingAreaMousePressEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
    QCompleter *c = nullptr;
    bool isLspFreeze = false;
    QWidget *m_popupWindow = nullptr;
    QListWidget *m_listWidget = nullptr;
    int m_startPosition = 0;
    FoldingArea *m_foldingArea = nullptr;
    int foldingAreaWidth() { return 16; }
};

class FoldingArea : public QWidget
{
public:
    FoldingArea(CodeEditor *editor) : QWidget(editor), m_editor(editor) {}
    QSize sizeHint() const override { return QSize(m_editor->foldingAreaWidth(), 0); }
protected:
    void paintEvent(QPaintEvent *event) override { m_editor->foldingAreaPaintEvent(event); }
    void mousePressEvent(QMouseEvent *event) override { m_editor->foldingAreaMousePressEvent(event); }
private:
    CodeEditor *m_editor;
};

#endif // CODEEDITOR_H
