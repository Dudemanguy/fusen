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

#include <fstream>
#include <iostream>
#include <ostream>

#include "sql.h"
#include "utils.h"

static int path_callback(void *data, int argc, char **argv, char **azColName) {
    QSet<QString> *paths = static_cast<QSet<QString> *>(data);
    for (int i = 0; i < argc; i++) {
        paths->insert(argv[i]);
    }
    return 0;
}

static void sql_clear_duplicates(sqlite3 *database) {
    char *err;
    std::string sql = std::string("DELETE FROM master WHERE rowid NOT IN (select min(rowid) from master GROUP BY path, tag)");
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while clearing duplicate rows!: " << err << std::endl;
    }
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
        std::string sql = std::string("CREATE TABLE master (") + \
                        "'index' INTEGER, 'path' TEXT, 'tag' TEXT," + \
                        "PRIMARY KEY('index' AUTOINCREMENT))";
        if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
            std::cerr << "Error while creating table: " << err << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    return database;
}

void sql_add_tags(sqlite3 *database, QStringList filenames, QStringList tags)
{
    char *err;
    std::string sql = std::string("INSERT INTO master (path, tag) VALUES ");
    for (int i = 0; i < filenames.size(); ++i) {
        std::string sql_query = sql + "('" + sanitize_path(filenames.at(i).toStdString()) + "','";
        for (int j = 0; j < tags.size(); ++j) {
             std::string sql_final = sql_query + tags.at(j).toStdString() + "')";
            if (sqlite3_exec(database, sql_final.c_str(), NULL, 0, &err)) {
                std::cerr << "Error while adding tags: " << err << std::endl;
            }
        }
    }
    sql_clear_duplicates(database);
}

void sql_clear_tags(sqlite3 *database, QStringList filenames) {
    // Just remove from the database and then readd the paths.
    sql_remove_paths(database, filenames);
    sql_add_paths(database, filenames);
}

bool sql_add_paths(sqlite3 *database, QStringList paths) {
    std::string sql = std::string("INSERT INTO master (path) VALUES ");
    for (int i = 0; i < paths.size(); ++i) {
        std::string str = sanitize_path(paths.at(i).toStdString());
        sql += "('" + str + "'),";
    }
    // Replace trailing comma
    sql.pop_back();
    sql += ';';
    char *err;
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while adding files: " << err << std::endl;
        return false;
    }
    return true;
}

QSet<QString> sql_get_paths(sqlite3 *database) {
    QSet<QString> paths;
    std::string sql = std::string("SELECT DISTINCT path FROM master");
    char *err;
    if (sqlite3_exec(database, sql.c_str(), path_callback, static_cast<void *>(&paths), &err)) {
        std::cerr << "Error while reading paths: " << err << std::endl;
    }
    return paths;
}

bool sql_remove_paths(sqlite3 *database, QStringList paths) {
    std::string sql = std::string("DELETE FROM master WHERE (path) IN (");
    for (int i = 0; i < paths.size(); ++i) {
        std::string str = sanitize_path(paths.at(i).toStdString());
        sql += "('" + str + "'),";
    }
    // Remove trailing comma
    sql.pop_back();
    sql += ");";
    char *err;
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while removing files: " << err << std::endl;
        return false;
    }
    return true;
}

void sql_remove_tags(sqlite3 *database, QStringList filenames, QStringList tags)
{
    char *err;
    std::string sql = std::string("DELETE FROM master WHERE path = '");
    for (int i = 0; i < filenames.size(); ++i) {
        std::string sql_query = sql + sanitize_path(filenames.at(i).toStdString()) + "' AND tag = '";
        for (int j = 0; j < tags.size(); ++j) {
            std::string sql_final = sql_query + tags.at(j).toStdString() + "'";
            if (sqlite3_exec(database, sql_final.c_str(), NULL, 0, &err)) {
                std::cerr << "Error while adding tags: " << err << std::endl;
            }
        }
    }
}

QSet<QString> sql_update_entries(sqlite3 *database, QStringList tags) {
    char *err;
    QSet<QString> entries;
    std::string sql = std::string("SELECT DISTINCT path FROM master ");
    for (int i = 0; i < tags.size(); ++i) {
        std::string tag = tags.at(i).toStdString();
        bool exclude = false;
        if (tags.at(i)[0] == '-') {
            tag = tags.at(i).mid(1).toStdString();
            exclude = true;
        }
        sql += exclude ? "EXCEPT " : "INTERSECT ";
        sql += "SELECT DISTINCT path FROM master WHERE tag = '" + tag + "' ";
    }
    if (sqlite3_exec(database, sql.c_str(), path_callback, static_cast<void *>(&entries), &err)) {
        std::cerr << "Error while reading entries: " << err << std::endl;
        entries.clear();
    }
    return entries;
}

void sql_write_database_contents(sqlite3 *database, std::string filename) {
    YAML::Emitter yaml;
    sqlite3_stmt *stmt;
    std::string sql = std::string("SELECT DISTINCT path,tag FROM master GROUP BY path,tag");
    int rc = sqlite3_prepare_v2(database, sql.c_str(), -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        std::cerr << "Error while getting database contents: " << sqlite3_errmsg(database) << std::endl;
        return;
    }

    yaml << YAML::BeginMap;
    int index = 0;
    while (true) {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            std::cerr << "Error while getting database contents: " << sqlite3_errmsg(database) << std::endl;
            break;
        }
        const char *path = (const char *)sqlite3_column_text(stmt, 0);
        if (sqlite3_column_type(stmt, 1) == SQLITE_NULL) {
            if (index != 0) {
                yaml << YAML::EndSeq;
            }
            yaml << YAML::Key << path;
            yaml << YAML::Value << YAML::BeginSeq;
        } else {
            yaml << (const char *)sqlite3_column_text(stmt, 1);
        }
        ++index;
    }
    yaml << YAML::EndSeq << YAML::EndMap;
    sqlite3_finalize(stmt);

    fs::path file = fs::path(filename);
    std::ofstream fout(file.string().c_str());
    fout << yaml.c_str();
    fout.close();
}
