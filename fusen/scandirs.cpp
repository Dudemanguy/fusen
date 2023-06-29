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

#include <QFileDialog>
#include <QGridLayout>
#include <QPushButton>

#include "scandirs.h"
#include "sql.h"
#include "utils.h"

ScanDirsWidget::ScanDirsWidget(sqlite3 *dbase, mainSettings *set, QWidget *parent) : QWidget(parent) {
    database = dbase;
    settings = set;

    scanDialog = new QDialog(this);
    scanList = new QListWidget(this);

    for (int i = 0; i < settings->scanDirs.size(); ++i) {
        new QListWidgetItem(tr(settings->scanDirs.at(i).toStdString().c_str()), scanList);
    }

    QPushButton *addDirectory = new QPushButton("Add Directory", this);
    QPushButton *removeDirectory = new QPushButton("Remove Directory", this);
    QPushButton *cancel = new QPushButton("Cancel", this);
    connect(addDirectory, &QPushButton::released, this, &ScanDirsWidget::addDirectory);
    connect(removeDirectory, &QPushButton::released, this, &ScanDirsWidget::removeDirectory);
    connect(cancel, &QPushButton::released, this, &ScanDirsWidget::cancel);

    QGridLayout *layout = new QGridLayout();
    layout->addWidget(scanList, 0, 0, 0, 3);
    layout->addWidget(addDirectory, 1, 0);
    layout->addWidget(removeDirectory, 1, 1);
    layout->addWidget(cancel, 1, 2);
    scanDialog->setLayout(layout);

    scanDialog->setWindowTitle("Scan Directories");
    scanDialog->show();
}

void ScanDirsWidget::addDirectory() {
    QSet<QString> existing_files = sql_get_paths(database);
    QString directory = QFileDialog::getExistingDirectory(this, "Add Directory");
    if (!directory.isEmpty()) {
        new QListWidgetItem(tr(directory.toStdString().c_str()), scanList);
        settings->scanDirs.append(directory);
        // Technically doesn't update the whole list but as soon as a user searches
        // something, the entries will be refreshed anyways so don't worry about it.
        scanDirectories(database, directory, existing_files);
    }
}

void ScanDirsWidget::cancel() {
    scanDialog->close();
}

void ScanDirsWidget::removeDirectory() {
    QString itemText = scanList->currentItem()->text();
    qDeleteAll(scanList->selectedItems());
    settings->scanDirs.removeOne(itemText);
}
