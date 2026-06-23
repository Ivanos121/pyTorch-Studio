#ifndef PROJECTROOTPROXYMODEL_H
#define PROJECTROOTPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QDir>
#include <QFileInfo>

class QIdentityProxyModel;
class ProjectRootProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

private:
    QString m_rootPath;
    QString m_projectName;

public:
    explicit ProjectRootProxyModel(QObject *parent = nullptr);

    // Метод инициализации данных проекта для подмены строки
    void setProjectInfo(const QString &rootPath, const QString &projectName);

    // Переопределенный метод получения данных для отображения корня
    QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

};

#endif // PROJECTROOTPROXYMODEL_H
