#ifndef AIPROJECTMODEL_H
#define AIPROJECTMODEL_H

#include <QAbstractItemModel>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QStyle>

class AIProjectModel : public QAbstractItemModel
{
    Q_OBJECT

private:
    QFileSystemModel *m_sourceModel;
    QString m_rootPath;
    QString m_projectName;
    QString m_pystudioPath;

public:
    explicit AIProjectModel(QObject *parent = nullptr);

    void setSourceModel(QFileSystemModel *sourceModel, const QString &rootPath);

    // Архитектурные методы перестройки дерева Qt
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // Взаимная трансляция индексов (Прокси <-> Диск)
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;
};

#endif // AIPROJECTMODEL_H
