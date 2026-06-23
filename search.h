#ifndef SEARCH_H
#define SEARCH_H

#include <QWidget>

namespace Ui {
class Search;
}

class Search : public QWidget
{
    Q_OBJECT

public:
    explicit Search(QWidget *parent = nullptr);
    ~Search();
    QString getSearchText() const;
    QString getReplaceText() const;

    bool isMatchCase() const;
    bool isWholeWords() const;
    bool isRegex() const;

signals:
    void searchParametersChanged();
    void findNextRequested();
    void findPrevRequested();
    void selectAllRequested();

    void replaceCurrentRequested();
    void replaceAndFindNextRequested();
    void replaceAllRequested();

private:
    Ui::Search *ui;
};

#endif // SEARCH_H
