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
#include <QProcess>
#include <QTimer>
#include <QPointer>
#include <QMetaType>
#include <QList>
#include <QHelpEvent>
#include <QToolTip>
#include <QTextBlock>
#include <QHBoxLayout>

class CodeEditor;
class Neuro_program;
class FoldingArea;
class MinimapArea;
class StickyScrollArea;

// =========================================================================
// ШАГ 1: СТРУКТУРА БЫСТРЫХ ИСПРАВЛЕНИЙ ПЕРЕНЕСЕНА НАВЕРХ (ДО КЛАССА EDITOR)
// =========================================================================
struct QuickFixAction {
    QString title;       // Что увидит пользователь, например: "Import os"
    QString newText;     // Какой текст вставить
    int startLine = 0;   // Координаты замены текста
    int startChar = 0;
    int endLine = 0;
    int endChar = 0;
};

class FolderBlockData : public QTextBlockUserData
{
public:
    int indentLevel = 0;     // Уровень отступа строки
    bool isFoldStart = false; // Начало блока (def/class)
    bool isFolded = false;    // Свернут ли блок

    enum ChangeState { Unchanged, Modified, Saved };
    ChangeState changeState = Unchanged;
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
    void highlightBlock(const QString &text) override;
private:
    QHash<QRegularExpression, QTextCharFormat*> highlightingRulesMap;
    QRegularExpression tripleSingleQuote;
    QRegularExpression tripleDoubleQuote;
};

class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor *editor);
    QSize sizeHint() const override { return QSize(0, 0); }
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    CodeEditor *codeEditor;
};

// =========================================================================
// ШАГ 2: ОСНОВНОЙ КЛАСС КОРРЕКТНО ВИДИТ ТИП ДАННЫХ QUICKFIXACTION
// =========================================================================
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
    friend class Neuro_programm;
    friend class FoldingArea;

public:
    CodeEditor(QWidget *parent = nullptr);
    virtual ~CodeEditor();
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void setCompleter(QCompleter *completer);
    QString textUnderCursor() const;
    QStringList temporaryOpenFilesBackup;
    void updateFoldingData();
    void registerCompletionWidgets(QWidget* popup, QListWidget* list);
    int getHorizontalOffset() const { return static_cast<int>(contentOffset().x()); }
    int getVerticalOffset() const { return static_cast<int>(contentOffset().y()); }
    int foldingAreaWidth() { return 16; }
    QTextBlock getFirstVisibleBlock() const { return firstVisibleBlock(); }
    int getFirstVisibleBlockTop() const {
        return static_cast<int>(blockBoundingGeometry(firstVisibleBlock()).top());
    }
    static QWidget* createEditorWithMinimap(QWidget *parent, CodeEditor* &outEditor, MinimapArea* &outMinimap);

    int getBlockTop(const QTextBlock &block) const {
        return static_cast<int>(blockBoundingGeometry(block).top());
    }
    void handleMouseMoveFromEditor(const QPoint &pos);
    void handleMouseLeaveFromEditor();
    void setChangesAsSaved();

    struct LspErrorData {
        int line;
        int startChar;
        int endChar;
        bool isError;
    };
    static QList<LspErrorData> currentLspErrors;
    QString currentFilePath;

    void formatSelectedPythonCode();
signals:
    void logMessage(const QString &message);
    void errorsCountChanged(int count);
    void selectionExecutionRequested(const QString &selectedText);
    void documentationRequested(const QString &filePath, int line, int character);

public slots:
    // Теперь этот слот скомпилируется без ошибок соответствия типов!
    void showQuickFixMenu(const QList<QuickFixAction>& fixes);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void foldingAreaPaintEvent(QPaintEvent *event);
    void foldingAreaMousePressEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *e) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);
    void onLspReadyRead();
    void sendLspDidChange();
    void applySelectionsFromLsp(const QList<QTextEdit::ExtraSelection> &selections);
    void matchBrackets();
    void showEditorContextMenu(const QPoint &pos);
    void onToggleCommentRequested();
    void onAutoIndentRequested();
    void onRunCurrentFileRequested();
    void onCheckSyntaxRequested();
    void showLspCompletionsInGui(const QStringList &completions);
    void drawLspErrorsInGui(const QList<CodeEditor::LspErrorData> &errors);

private:
    QCompleter *c = nullptr;
    bool m_isInsertingMultiCaret = false;
    qint64 m_lastKeyPressTime = 0;
    bool isLspFreeze = false;
    QPointer<QWidget> m_popupWindow;
    QListWidget *m_listWidget = nullptr;
    int m_startPosition = 0;
    FoldingArea *m_foldingArea = nullptr;
    StickyScrollArea *m_stickyScrollWidget = nullptr;
    QProcess *lspProcess = nullptr;
    QTimer *lspDelayTimer = nullptr;
    void clearErrorHighlights();
    void highlightError(int startLine, int startChar, int endLine, int endChar, bool isError);
    int findMatchingBracket(int pos, QChar openBracket, QChar closeBracket, bool directionRight);

    QList<QTextEdit::ExtraSelection> lspExtraSelections;
    PythonHighlighter *m_highlighter = nullptr;
    int lspDocumentVersion = 1;
    QList<QTextEdit::ExtraSelection> m_lspSelectionsBuffer;
    QList<QTextEdit::ExtraSelection> m_currentLspSelections;
    void sendLspDidOpen();
    QByteArray m_lspBuffer;
    QWidget *lineNumberArea;
    QWidget *m_foldingAreas;
    MinimapArea *minimapArea;
    QList<QTextCursor> m_virtualCursors;
    void updateVirtualCursorHighlights();
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

// =========================================================================
// ШАГ 3: РЕГИСТРАЦИЯ МЕТАТИПА ОСТАЕТСЯ В САМОМ НИЗУ ВНЕ КЛАССОВ
// =========================================================================
Q_DECLARE_METATYPE(QuickFixAction)

#endif // CODEEDITOR_H
