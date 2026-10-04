#pragma once

#include <QIdentityProxyModel>

class ProjectIndicatorsProxyModel : public QIdentityProxyModel {
    Q_OBJECT

public:
    explicit ProjectIndicatorsProxyModel(QObject *parent = nullptr);
    ~ProjectIndicatorsProxyModel() override = default;

    // Перехват и подмена отображаемых данных для первой колонки (Имя проекта)
    QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &proxyIndex, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
};
