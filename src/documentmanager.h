#ifndef DOCUMENTMANAGER_H
#define DOCUMENTMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include "codeeditor.h"

class DocumentManager : public QObject
{
    Q_OBJECT
public:
    explicit DocumentManager(QMainWindow *mainWindow, QComboBox *fileCombo, QListWidget *openFilesList, QLabel *titleLbl, QObject *parent = nullptr);

    // Точки входа при действиях с файлами
    void registerNewOpenFile(const QString &absoluteFilePath, CodeEditor *editor);
    void handleFileActivation(const QString &absoluteFilePath);
    void handleFileClosed(const QString &absoluteFilePath);
    void handleDocumentModificationChanged(const QString &absoluteFilePath, bool isModified);

    // Служебный метод принудительного обновления всего UI заголовков
    void updateUiTitles(const QString &absoluteFilePath);

private:
    QMainWindow *m_window;
    QComboBox *m_fileCombo;
    QListWidget *m_filesListWidget;
    QLabel *m_titleLabel;

    QString m_activeFilePath;
    QStringList m_openedFiles;

    QString getCleanProjectName() const;
};

#endif // DOCUMENTMANAGER_H
