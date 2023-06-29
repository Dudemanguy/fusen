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

#include <iostream>
#include <ostream>

#include "sql.h"
#include "utils.h"

static int col_callback(void *data, int argc, char **argv, char **azColName) {
    QSet<QString> *columns = static_cast<QSet<QString> *>(data);
    for (int i = 0; i < argc; i++) {
        if (strcmp(azColName[i], "name") == 0) {
            columns->insert(argv[i]);
        }
    }
    return 0;
}

static int entry_callback(void *data, int argc, char **argv, char **azColName) {
    QSet<QString> *entries = static_cast<QSet<QString> *>(data);
    for (int i = 0; i < argc; i++) {
        entries->insert(argv[i]);
    }
    return 0;
}

QStringList build_entries(sqlite3 *database) {
    QSet<QString> entries;
    std::string sql = std::string("SELECT ") + PRIMARY_KEY + std::string(" FROM ") + TABLE;
    char *err;
    if (sqlite3_exec(database, sql.c_str(), entry_callback, static_cast<void *>(&entries), &err)) {
        std::cerr << "Error while trying to read table: " << err << std::endl;
    }
    QStringList list(entries.begin(), entries.end());
    return list;
}

sqlite3 *connectDatabase() {
    fs::path file = getUserFile("data");

    bool create_table = false;
    if (!fs::exists(file)) {
        create_table = true;
        fs::create_directories(file.parent_path());
    }

    sqlite3 *database;
    int ret = sqlite3_open(file.string().c_str(), &database);
    if (ret != SQLITE_OK) {
        std::cerr << "Error while trying to open database: " << sqlite3_errstr(ret) << std::endl;
        exit(EXIT_FAILURE);
    }

    if (create_table) {
        char *err;
        std::string sql = std::string("CREATE TABLE ") + TABLE + std::string("(") + \
                        PRIMARY_KEY + std::string(" VARCHAR(1024) PRIMARY KEY)");
        if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
            std::cerr << "Error while creating table: " << err << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    return database;
}

void sql_add_columns(sqlite3 *database, std::string key, QStringList columns) {
    char *err;
    // Has to be done one column at a time.
    for (int i = 0; i < columns.size(); ++i) {
        std::string sql = std::string("ALTER TABLE ") + TABLE + " ADD COLUMN " +
                          "'" + columns.at(i).toStdString() + "' INTEGER";
        if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
            std::cerr << "Error while adding columns: " << err << std::endl;
        }
    }
}

void sql_clear_tags(sqlite3 *database, QSet<QString> columns, QStringList filenames) {
    char *err;
    std::string sql = std::string("UPDATE ") + TABLE + " SET ";
    // Don't clear primary key
    std::string primary_key = std::string(PRIMARY_KEY);
    columns.remove(PRIMARY_KEY);
    for (auto i = columns.begin(), end = columns.end(); i != end; ++i) {
        sql += "'" + (*i).toStdString() + "' = NULL,";
    }
    // Remove trailing comma
    sql.pop_back();
    sql += " WHERE ";
    for (int i = 0; i < filenames.size(); ++i) {
        QString filename = filenames[i].replace("\'", "\'\'");
        sql += primary_key + " = '" + filenames[i].toStdString() + "' OR ";
    }
    sql.erase(sql.end() - 3, sql.end());
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while clearing tags: " << err << std::endl;
    }
}

QStringList sql_delete_paths(sqlite3 *database, std::string key, QStringList values) {
    QStringList entries;
    std::string sql = std::string("DELETE FROM ") + TABLE + " WHERE " + key + " IN (";
    for (int i = 0; i < values.size(); ++i) {
        std::string str = values.at(i).toStdString();
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '\'') {
                str.insert(i, "'");
                ++i;
            }
        }
        sql += "('" + str + "'),";
        entries.append(values.at(i));
    }
    // Remove trailing comma
    sql.pop_back();
    sql += ");";
    char *err;
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while removing files: " << err << std::endl;
        entries.clear();
    }
    return entries;
}

QSet<QString> sql_get_columns(sqlite3 *database) {
    QSet<QString> columns;
    std::string sql = std::string("PRAGMA table_info (") + TABLE + std::string(")");
    char *err;
    if (sqlite3_exec(database, sql.c_str(), col_callback, static_cast<void *>(&columns), &err)) {
        std::cerr << "Error while reading columns: " << err << std::endl;
    }
    return columns;
}

QSet<QString> sql_get_paths(sqlite3 *database) {
    QSet<QString> paths;
    std::string sql = std::string("SELECT ") + PRIMARY_KEY + " FROM " + TABLE;
    char *err;
    if (sqlite3_exec(database, sql.c_str(), entry_callback, static_cast<void *>(&paths), &err)) {
        std::cerr << "Error while reading paths: " << err << std::endl;
    }
    return paths;
}

QStringList sql_insert_into(sqlite3 *database, std::string key, QStringList values) {
    QStringList entries;
    std::string sql = std::string("INSERT INTO ") + TABLE + " (" + key + ") VALUES ";
    for (int i = 0; i < values.size(); ++i) {
        std::string str = values.at(i).toStdString();
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '\'') {
                str.insert(i, "'");
                ++i;
            }
        }
        sql += "('" + str + "'),";
        entries.append(values.at(i));
    }
    // Replace trailing comma
    sql.pop_back();
    sql += ';';
    char *err;
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while adding files: " << err << std::endl;
        entries.clear();
    }
    return entries;
}

QSet<QString> sql_update_entries(sqlite3 *database, QStringList tags, QSet<QString> columns) {
    char *err;
    QSet<QString> entries;
    std::string sql = std::string("SELECT ") + PRIMARY_KEY + " FROM " + TABLE + " WHERE ";
    for (int i = 0; i < tags.size(); ++i) {
        std::string tag = tags.at(i).toStdString();
        std::string include = " NOT ";
        if (tags.at(i)[0] == '-') {
            include = "";
            tag = tags.at(i).mid(1).toStdString();
            if (!columns.contains(tag.c_str())) {
                continue;
            }
        }
        sql += "\"" + tag + "\" IS" + include + "NULL AND ";
    }
    sql.erase(sql.end() - 5, sql.end());
    if (sqlite3_exec(database, sql.c_str(), entry_callback, static_cast<void *>(&entries), &err)) {
        std::cerr << "Error while reading entries: " << err << std::endl;
        entries.clear();
    }
    return entries;
}

void sql_update_tags(sqlite3 *database, std::string key,
                     QStringList filenames, QStringList tags, bool add)
{
    char *err;
    std::string value = add ? "1" : "NULL";
    std::string sql = std::string("UPDATE ") + TABLE + " SET ";
    for (int i = 0; i < tags.size(); ++i) {
        sql += "'" + tags.at(i).toStdString() + "' = " + value + ", ";
    }
    // Remove trailing comma
    sql.erase(sql.end() - 2, sql.end());
    sql += " WHERE ";
    for (int i = 0; i < filenames.size(); ++i) {
        QString filename = filenames[i].replace("\'", "\'\'");
        sql += key + " = '" + filename.toStdString() + "' OR ";
    }
    sql.erase(sql.end() - 3, sql.end());
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while adding tags: " << err << std::endl;
    }
}
