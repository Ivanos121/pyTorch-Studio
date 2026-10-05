#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QTableView>
#include <QSqlTableModel>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QSqlDatabase>

class all_spisok_progects_panel : public QWidget {
    Q_OBJECT

public:
    explicit all_spisok_progects_panel(QWidget *parent = nullptr);
    ~all_spisok_progects_panel() override = default;

    // Метод инициализации
    void initPanel(const QString& dbPath, const QJsonObject& layoutConfig);
    void refreshPanel();
    static int countFilesRecursive(const QString& dirPath, const QStringList& nameFilters);
    void ensureLocalProjectDatabase(const QString& projectPath);

signals:
    void requestOpenProject(const QString& projectPath);
    void requestChangeStatus(const QString& projectName);
    void requestSystemNotification(const QString& title, const QString& message);


protected:
    // Перехватывает абсолютно любые изменения состояния виджета в системе Qt6
    bool event(QEvent *event) override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // Функция запуска сборки интерфейса
    void buildUiFromJson(const QJsonObject& layoutConfig);
    void scanProjectsDirectory(const QString& targetDirPath);
    QLayout* parseLayout(const QJsonObject& layoutObj);
    QWidget* parseWidget(const QJsonObject& widgetObj);
    void setupTableView(QTableView* tableView, const QJsonObject& dataSourceObj);
    void showContextMenu(const QModelIndex& proxyIndex, const QPoint& globalPos);
    void handleAction(const QString& actionId);
    void applyLiveSearch(const QString& text);
    void applyStatusFilter(int index);
    void updateContextPath();

    QString m_dbPath;
    QSqlDatabase m_db;
    QSqlTableModel* m_tableModel = nullptr;
    QTableView* m_tableView = nullptr;
    QLabel* m_lblPathContext = nullptr;
    QLabel* m_lblCounter = nullptr;
    QComboBox* m_cmbStatusFilter = nullptr;
};
