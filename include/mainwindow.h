#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLineEdit>
#include <QMainWindow>
#include <sqlite3.h>

struct mainSettings {
    QAction *clearTags;
    std::string defaultApplicationPath;
};

class MainWindow : public QMainWindow {
    public:
        explicit MainWindow(QWidget *parent = 0);
        mainSettings *settings;
        sqlite3 *database;
        QAction *clearTags;
        QDialog *defaultOpen;
        QLineEdit *defaultOpenWith;
        QStringList entries;
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
        void defaultApplicationOpen();
        void importTags();
        void openFiles();
        void removeFiles();
        void tagFiles();
        void updateApplication(bool update);
        void updateEntries(const QString str);
        void updateNameCheck(bool checked);
        void updateTags(bool add);
};

#endif
