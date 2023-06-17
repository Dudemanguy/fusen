#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLineEdit>
#include <QMainWindow>
#include <sqlite3.h>

struct mainSettings {
    QAction *importSettings;
};

class MainWindow : public QMainWindow {
    public:
        explicit MainWindow(QWidget *parent = 0);
        mainSettings *settings;
        sqlite3 *database;
        QStringList entries;
        QAction *importSettings;
        QListView *listView;
        QStringListModel *model;
        QLineEdit *searchBox;
        QCheckBox *searchNames;
        QDialog *tagDialog;
        QLineEdit *tagEdit;
    private slots:
        void addDirectory(bool recursive);
        void addFiles();
        void closeEvent(QCloseEvent *event);
        void openFiles();
        void removeFiles();
        void tagFiles();
        void updateEntries(const QString str);
        void updateNameCheck(bool checked);
        void updateTags(bool add);
};

#endif
