#pragma once

#include <QWidget>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include <qboxlayout.h>

// Опережающие объявления для разгрузки заголовка
class QLabel;
class QPushButton;
class QSplitter;

namespace QtCharts {
class QChartView;
class QLineSeries;
class QValueAxis;
}

// =========================================================================
// КЛАСС-СБОРЩИК ПРОФЕССИОНАЛЬНОГО ИНТЕРФЕЙСА ИИ-МОНИТОРИНГА ПАК
// =========================================================================
class ThermalMonitorBuilder : public QWidget {
    Q_OBJECT
public:
    explicit ThermalMonitorBuilder(QWidget *parent = nullptr);
    ~ThermalMonitorBuilder();

    // Главный метод сборки интерфейса по JSON схеме
    bool buildMonitorUi(const QString &schemaPath);

    // Быстрый доступ к созданным виджетам по их именам из JSON
    QWidget* getWidgetByName(const QString &name) const;

private:
    // Метод парсинга одиночного элемента
    void parseAndLayoutElement(const QJsonObject &elementObj, QWidget *parentWidget, void *targetLayoutOrSplitter);

    QMap<QString, QWidget*> m_monitorWidgetsMap;
    QVBoxLayout *m_mainLayout = nullptr;
};
