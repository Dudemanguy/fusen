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

#include <cassert>
#include <cstring>
#include <iostream>
#include <pwd.h>
#include <unistd.h>

#include "sql.h"
#include "utils.h"

QStringList getNewDirectoryFiles(QString directory, QSet<QString> existing_files, bool recursive) {
    QStringList filenames;
    if (!directory.isEmpty()) {
        fs::path path = fs::path(directory.toStdString());
        if (recursive) {
            for (auto& p: fs::recursive_directory_iterator(path)) {
                if (p.is_regular_file() and !existing_files.contains(p.path().string().c_str())) {
                    filenames.append(p.path().string().c_str());
                }
            }
        } else {
            for (auto& p: fs::directory_iterator(path)) {
                if (p.is_regular_file() and !existing_files.contains(p.path().string().c_str())) {
                    filenames.append(p.path().string().c_str());
                }
            }
        }
    }
    return filenames;
}

fs::path getUserFile(const char *type) {
    const char *homedir;
    const char *filename = NULL;
    if (strcmp(type, "data") == 0) {
        filename = "data.sqlite";
    } else if (strcmp(type, "settings") == 0) {
        filename = "settings.yaml";
    }

    assert(filename && filename[0]);

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    if (homedir == NULL) {
        std::cerr << "Couldn't locate the user's home directory. Exiting!" << std::endl;
        exit(EXIT_FAILURE);
    }
    fs::path file = fs::path(homedir) / ".local" / "share" / "fusen" / filename;

    return file;
}

bool scanDirectories(sqlite3 *database, QString directory, QSet<QString> existing_files) {
    QStringList filenames = getNewDirectoryFiles(directory, existing_files, true);
    if (!filenames.isEmpty()) {
        return sql_insert_into(database, std::string(PRIMARY_KEY), filenames);
    }
    return true;
}
