/* This file is a part of fusen.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCheckBox>
#include <QLineEdit>
#include <QMainWindow>
#include <QStringListModel>
#include <sqlite3.h>

#include "utils.h"

class MainWindow : public QMainWindow {
    public:
        explicit MainWindow(QWidget *parent = 0);
    private:
        QAction *clearTags;
        sqlite3 *database;
        QDialog *defaultOpen;
        QLineEdit *defaultOpenWith;
        QStringList entries;
        QListView *listView;
        QStringListModel *model;
        QDialog *openWith;
        QLineEdit *openWithEntry;
        QLineEdit *searchBox;
        QCheckBox *searchNames;
        mainSettings *settings;
        QDialog *tagDialog;
        QLineEdit *tagEdit;
    private slots:
        void addDirectory(bool recursive);
        void addFiles();
        void addScanDirs();
        void closeEvent(QCloseEvent *event);
        void copyPath();
        void defaultApplicationOpen();
        void exportTags();
        void importTags();
        void openFiles(bool defaultApplication);
        void openFilesWith();
        void removeFiles();
        void tagFiles();
        void updateApplication(bool update);
        void updateEntries(const QString str);
        void updateNameCheck(bool checked);
        void updateTags(bool add);
};

#endif
