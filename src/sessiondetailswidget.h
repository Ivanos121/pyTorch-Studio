#pragma once

#include <QWidget>
#include <QPushButton>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QSettings>
#include <QDateTime>
#include <QStackedWidget>
#include <QUrl>
#include <QPixmap>
#include <QWebEngineView>
#include <QResizeEvent>
#include <QProcess>

QT_BEGIN_NAMESPACE
namespace Ui { class SessionDetailsWidget; }
QT_END_NAMESPACE

class SessionDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SessionDetailsWidget(QWidget *parent = nullptr);
    ~SessionDetailsWidget() override;
    void loadSession(const QString &projectPath, const QString &sessionId);
    void clearPanel();

signals:
    void backToJournalRequested();

private:
    void setWebMaximizeMode(bool maximized);
    void resizeEvent(QResizeEvent *event) override;

    Ui::SessionDetailsWidget *ui;
    QWebEngineView           *m_webView;
    QString                   m_currentSessionId;
    QString                   m_currentGraphPath;
    QJsonArray                m_webServicesConfig;
    QProcess                 *m_activeWebServiceProcess;
    QString                   m_currentProjectPath;
    qint64                    m_activeWebServicePid;
};
