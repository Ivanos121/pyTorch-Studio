#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QFormLayout>
#include <QJsonObject>
#include <QMap>

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog() = default;

private:
    void setupUi();
    void loadSchemaAndBuildUi();
    QWidget* createWidgetForType(const QJsonObject &settingObj);

    // Компоненты каркаса
    QListWidget *m_listWidget;
    QStackedWidget *m_stackedWidget;

    // Карта для отслеживания слоев страниц под каждую категорию
    QMap<QString, QFormLayout*> m_categoryLayouts;
};
