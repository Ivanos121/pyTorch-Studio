#include "thermalmonitorbuilder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QDebug>

#include <QChartView>
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>

ThermalMonitorBuilder::ThermalMonitorBuilder(QWidget *parent)
    : QWidget(parent)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
}

ThermalMonitorBuilder::~ThermalMonitorBuilder()
{
    m_monitorWidgetsMap.clear();
}

QWidget* ThermalMonitorBuilder::getWidgetByName(const QString &name) const
{
    return m_monitorWidgetsMap.value(name, nullptr);
}

bool ThermalMonitorBuilder::buildMonitorUi(const QString &schemaPath)
{
    QFile file(schemaPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "❌ [MonitorBuilder]: Не удалось открыть:" << schemaPath;
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "❌ [MonitorBuilder]: Ошибка JSON:" << parseError.errorString();
        return false;
    }

    QJsonObject rootObj = doc.object();
    QJsonArray elementsArray = rootObj[QStringLiteral("elements")].toArray();

    for (int i = 0; i < elementsArray.size(); ++i) {
        QJsonObject elem = elementsArray[i].toObject();
        if (elem[QStringLiteral("component")].toString() == QStringLiteral("QSplitter")) {

            // Создаем горизонтальный сплиттер ПАК
            QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
            QString splitterName = elem[QStringLiteral("object_name")].toString();
            splitter->setObjectName(splitterName);
            if (elem.contains(QStringLiteral("style_sheet"))) splitter->setStyleSheet(elem[QStringLiteral("style_sheet")].toString());

            // ВАЖНО: Разрешаем сплиттеру растягиваться на весь экран Студии
            splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_mainLayout->addWidget(splitter);
            m_monitorWidgetsMap[splitterName] = splitter;

            QJsonArray children = elem[QStringLiteral("children")].toArray();
            for (int j = 0; j < children.size(); ++j) {
                QJsonObject childContainer = children[j].toObject();

                QWidget *containerWidget = new QWidget(splitter);
                QVBoxLayout *containerLayout = new QVBoxLayout(containerWidget);
                containerLayout->setSpacing(childContainer[QStringLiteral("spacing")].toInt(12));
                containerLayout->setContentsMargins(0, 0, 0, 0);

                // Настраиваем Expanding политику для контейнеров левой и правой части
                containerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

                QJsonArray widgetsArray = childContainer[QStringLiteral("widgets")].toArray();
                for (int k = 0; k < widgetsArray.size(); ++k) {
                    QJsonObject wObj = widgetsArray[k].toObject();
                    QString type = wObj[QStringLiteral("type")].toString();
                    QString name = wObj[QStringLiteral("object_name")].toString();
                    QString textVal = wObj[QStringLiteral("text")].toString();

                    if (type == QStringLiteral("QLabel")) {
                        QLabel *label = new QLabel(textVal, containerWidget);
                        label->setObjectName(name);

                        if (name == QStringLiteral("lblVideoCanvas")) {
                            // ФИКС №1: Левое видеоокно занимает ВСЁ свободное пространство
                            label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                            label->setScaledContents(true); // Картинка OpenCV будет плавно растягиваться!
                        } else {
                            // Табло температуры ИИ сверху справа
                            label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                            label->setMaximumHeight(50); // Ужимаем верхний виджет метрики по высоте
                        }

                        if (wObj[QStringLiteral("alignment")].toString() == QStringLiteral("AlignCenter")) label->setAlignment(Qt::AlignCenter);
                        if (wObj.contains(QStringLiteral("style_sheet"))) label->setStyleSheet(wObj[QStringLiteral("style_sheet")].toString());

                        containerLayout->addWidget(label);
                        m_monitorWidgetsMap[name] = label;
                    }
                    else if (type == QStringLiteral("QPushButton")) {
                        QPushButton *btn = new QPushButton(textVal, containerWidget);
                        btn->setObjectName(name);
                        btn->setCursor(Qt::PointingHandCursor);
                        btn->setEnabled(true);

                        // =========================================================================
                        // ЖЕСТКИЙ ФИКС: РАСПОЗНАВАНИЕ ТУМБЛЕРА ПРИ ЛЮБОМ СИНТАКСИСЕ JSON
                        // =========================================================================
                        if (wObj.contains(QStringLiteral("checkable"))) {
                            QJsonValue chVal = wObj[QStringLiteral("checkable")];
                            if (chVal.isBool()) {
                                btn->setCheckable(chVal.toBool());
                            } else {
                                // Если в JSON написано "true" или "true," в кавычках — всё равно включаем тумблер!
                                QString strVal = chVal.toString().trimmed().toLower();
                                btn->setCheckable(strVal.startsWith(QStringLiteral("true")));
                            }
                        } else {
                            // Если это кнопка записи на нашей странице, принудительно делаем её чекаемой
                            if (name == QStringLiteral("btnRecordVideo")) {
                                btn->setCheckable(true);
                            }
                        }
                        // =========================================================================

                        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                        btn->setFixedHeight(35);
                        btn->setFixedWidth(260);
                        if (wObj.contains(QStringLiteral("style_sheet"))) {
                            btn->setStyleSheet(wObj[QStringLiteral("style_sheet")].toString());
                        }

                        containerLayout->addWidget(btn, 0, Qt::AlignHCenter);
                        m_monitorWidgetsMap[name] = btn;
                    }

                    else if (type == QStringLiteral("QChartView")) {
                        QChart *chart = new QChart();
                        QLineSeries *series = new QLineSeries();
                        chart->addSeries(series);
                        chart->legend()->hide();

                        QJsonObject props = wObj[QStringLiteral("chart_properties")].toObject();
                        chart->setTitle(props[QStringLiteral("title")].toString());

                        // Горизонтальная ось X (Время)
                        QValueAxis *axisX = new QValueAxis();
                        QJsonObject jX = props[QStringLiteral("axis_x")].toObject();
                        axisX->setTitleText(jX[QStringLiteral("label")].toString());
                        QJsonArray rX = jX[QStringLiteral("range")].toArray();
                        // ИСПРАВЛЕНО: Извлекаем значения по индексам 0 и 1 массива JSON
                        if (rX.size() == 2) {
                            axisX->setRange(rX[0].toDouble(), rX[1].toDouble());
                        }
                        chart->addAxis(axisX, Qt::AlignBottom);
                        series->attachAxis(axisX);

                        // Вертикальная ось Y (Градусы)
                        QValueAxis *axisY = new QValueAxis();
                        QJsonObject jY = props[QStringLiteral("axis_y")].toObject();
                        axisY->setTitleText(jY[QStringLiteral("label")].toString());
                        QJsonArray rY = jY[QStringLiteral("range")].toArray();
                        // ИСПРАВЛЕНО: Извлекаем значения по индексам 0 и 1 массива JSON
                        if (rY.size() == 2) {
                            axisY->setRange(rY[0].toDouble(), rY[1].toDouble());
                        }
                        chart->addAxis(axisY, Qt::AlignLeft);
                        series->attachAxis(axisY);

                        QChartView *chartView = new QChartView(chart, containerWidget);
                        chartView->setObjectName(name);
                        chartView->setRenderHint(QPainter::Antialiasing);

                        // ФИКС №2: Разрешаем графику расширяться на ВСЮ оставшуюся высоту и ширину
                        chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

                        if (wObj.contains(QStringLiteral("style_sheet"))) chartView->setStyleSheet(wObj[QStringLiteral("style_sheet")].toString());

                        chartView->setProperty("data_series", QVariant::fromValue(series));
                        chartView->setProperty("axis_x", QVariant::fromValue(axisX));

                        containerLayout->addWidget(chartView);
                        m_monitorWidgetsMap[name] = chartView;
                    }
                }

                // ВНИМАНИЕ: Сюда мы НЕ добавляем addStretch(1), чтобы макет не сжимал график и видео!
                splitter->addWidget(containerWidget);
            }

            // Жестко выставляем пропорцию сплиттера: 60% лево, 40% право
            splitter->setSizes(QList<int>({600, 400}));
        }
    }
    return true;
}
