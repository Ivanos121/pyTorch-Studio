#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>
#include <QString>

class ElidedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ElidedLabel(QWidget *parent = nullptr);
    void setFullText(const QString &text); // Используем этот метод вместо setText()

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_fullText; // Храним оригинальный длинный текст
};

#endif // ELIDEDLABEL_H
