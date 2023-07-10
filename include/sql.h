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

sqlite3 *connectDatabase();
void sql_add_columns(sqlite3 *database, std::string key, QStringList columns);
bool sql_add_paths(sqlite3 *database, QStringList paths);
void sql_add_tags(sqlite3 *database, QStringList filenames, QStringList tags);
void sql_clear_tags(sqlite3 *database, QStringList filenames);
QSet<QString> sql_get_columns(sqlite3 *database);
QSet<QString> sql_get_paths(sqlite3 *database);
bool sql_remove_paths(sqlite3 *database, QStringList paths);
void sql_remove_tags(sqlite3 *database, QStringList filenames, QStringList tags);
QSet<QString> sql_update_entries(sqlite3 *database, QStringList tags);

#endif
