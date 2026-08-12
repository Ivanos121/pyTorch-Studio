#ifndef DYNAMICFORMBUILDER_H
#define DYNAMICFORMBUILDER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDebug>

class DynamicFormBuilder : public QObject {
    Q_OBJECT
public:
    // Паттерн Singleton для глобального доступа из любой точки приложения
    static DynamicFormBuilder& instance();

    // Главный метод: читает схему интерфейса и строит виджеты внутри переданного Layout
    bool buildLayoutFromJson(const QString& schemaPath, QVBoxLayout* mainLayout);

    // Метод автоматической сборки и сохранения параметров в config/hyperparameters.yaml
    bool saveFieldsToYaml(const QString& projectPath);

    // Вспомогательный метод для очистки контейнера при перерисовке формы
    void clearLayout(QLayout* layout);

private:
    DynamicFormBuilder() = default;
    ~DynamicFormBuilder() override = default;
    DynamicFormBuilder(const DynamicFormBuilder&) = delete;
    DynamicFormBuilder& operator=(const DynamicFormBuilder&) = delete;

    // Ассоциативная карта для хранения указателей на созданные виджеты по их именам из JSON
    QMap<QString, QWidget*> m_widgetsMap;
};

#endif // DYNAMICFORMBUILDER_H
