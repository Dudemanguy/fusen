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

#ifndef SCANDIRS_H
#define SCANDIRS_H

#include <QDialog>
#include <QListWidget>
#include <QWidget>
#include <sqlite3.h>

#include "mainwindow.h"
#include "utils.h"

class ScanDirsWidget : public QWidget {
    public:
        explicit ScanDirsWidget(sqlite3 *dbase, mainSettings *set, QWidget *parent = 0);

    private:
        mainSettings *settings;
        sqlite3 *database;
        QDialog *scanDialog;
        QListWidget *scanList;

    private slots:
        void addDirectory();
        void cancel();
        void removeDirectory();
    //    void itemSelected( int item );
};

#endif
