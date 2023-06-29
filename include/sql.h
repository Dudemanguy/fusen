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

#ifndef SQL_H
#define SQL_H

#include <QSet>
#include <QStringList>
#include <sqlite3.h>

#define TABLE "master"
#define PRIMARY_KEY "path"

sqlite3 *connectDatabase();
QStringList build_entries(sqlite3 *database);
void sql_add_columns(sqlite3 *database, std::string key, QStringList columns);
void sql_clear_tags(sqlite3 *database, QSet<QString> columns, QStringList filenames);
QStringList sql_delete_paths(sqlite3 *database, std::string key, QStringList values);
QSet<QString> sql_get_columns(sqlite3 *database);
QSet<QString> sql_get_paths(sqlite3 *database);
bool sql_insert_into(sqlite3 *database, std::string key, QStringList values);
QSet<QString> sql_update_entries(sqlite3 *database, QStringList tags, QSet<QString> columns);
void sql_update_tags(sqlite3 *database, std::string key,
                     QStringList filenames, QStringList tags, bool add);

#endif
