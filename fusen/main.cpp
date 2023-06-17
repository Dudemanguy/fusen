#include <algorithm>
#include <filesystem>
#include <iostream>
#include <pwd.h>
#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QListView>
#include <QMenuBar>
#include <QPushButton>
#include <QSet>
#include <QStringListModel>
#include <unistd.h>

#include "mainwindow.h"

#define TABLE "master"
#define PRIMARY_KEY "path"

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");

    MainWindow window;
    window.show();

    return app.exec();
}

static int entry_callback(void *data, int argc, char **argv, char **azColName) {
    QStringList *entries = static_cast<QStringList *>(data);
    for (int i = 0; i < argc; i++) {
        *entries << argv[i];
    }
    return 0;
}

static int col_callback(void *data, int argc, char **argv, char **azColName) {
    QSet<QString> *columns = static_cast<QSet<QString> *>(data);
    for (int i = 0; i < argc; i++) {
        if (strcmp(azColName[i], "name") == 0) {
            columns->insert(argv[i]);
        }
    }
    return 0;
}

static QStringList build_entries(sqlite3 *database) {
    QStringList entries;
    std::string sql = std::string("SELECT ") + PRIMARY_KEY + std::string(" FROM ") + TABLE;
    char *err;
    if (sqlite3_exec(database, sql.c_str(), entry_callback, static_cast<void *>(&entries), &err)) {
        std::cerr << "Error while trying to read table: " << err << std::endl;
    }
    return entries;
}

static sqlite3 *connectDatabase() {
    const char *homedir;
    bool create_table = false;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    if (homedir == NULL) {
        std::cerr << "Couldn't locate the user's home directory. Exiting!" << std::endl;
        exit(EXIT_FAILURE);
    }
    fs::path file = fs::path(homedir) / ".local" / "share" / "fusen" / "data.sqlite";
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

static QStringList getSelectedFiles(QListView *listView) {
    QModelIndexList indexes = listView->selectionModel()->selectedIndexes();
    QStringList list;
    foreach (const QModelIndex &index, indexes) {
        list << index.data(Qt::DisplayRole).toString();
    }
    return list;
}

std::string sanitize_tags(std::string str) {
    // Replace some special characters and other nonsense for sanity
    // An sql injection is probably still possible but whatever don't make drop table a tag
    std::replace(str.begin(), str.end(), ' ', '_');
    std::replace(str.begin(), str.end(), '\'', '_');
    std::replace(str.begin(), str.end(), '"', '_');
    return str;
}

static QStringList splitTags(std::string str, char delimeter) {
    std::string sanitized_str = sanitize_tags(str);
    std::stringstream ss(sanitized_str);
    std::string item;
    QStringList split;
    while (std::getline(ss, item, delimeter)) {
        // Silently skip this since it can never work
        if (item.compare(PRIMARY_KEY) == 0)
            continue;
        split.append(item.c_str());
    }
    return split;
}

static void sql_add_columns(sqlite3 *database, std::string key, QStringList columns) {
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

static QSet<QString> sql_get_columns(sqlite3 *database) {
    QSet<QString> columns;
    std::string sql = std::string("PRAGMA table_info (") + TABLE + std::string(")");
    char *err;
    if (sqlite3_exec(database, sql.c_str(), col_callback, static_cast<void *>(&columns), &err)) {
        std::cerr << "Error while reading columns: " << err << std::endl;
    }
    return columns;
}

static void sql_update_tags(sqlite3 *database, std::string key,
                            QStringList filenames, QStringList tags,
                            bool add)
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
        sql += key + " = '" + filenames.at(i).toStdString() + "' OR ";
    }
    sql.erase(sql.end() - 3, sql.end());
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while adding tags: " << err << std::endl;
    }
}

static QStringList sql_delete(sqlite3 *database, std::string key, QStringList values) {
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

static QStringList sql_insert_into(sqlite3 *database, std::string key, QStringList values) {
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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    database = connectDatabase();

    QMenu *menu = menuBar()->addMenu(tr("&File"));

    QAction *addFiles = new QAction(tr("&Add Files"), this);
    addFiles->setStatusTip(tr("Add Files"));
    connect(addFiles, &QAction::triggered, this, &MainWindow::addFiles);
    menu->addAction(addFiles);

    QAction *addDirectory = new QAction(tr("&Add Directory"), this);
    addDirectory->setStatusTip(tr("Add Directory"));
    connect(addDirectory, &QAction::triggered, this, &MainWindow::addDirectory);
    menu->addAction(addDirectory);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    entries = build_entries(database);
    model = new QStringListModel(this);
    model->setStringList(entries);

    QLineEdit *searchBox = new QLineEdit(this);
    searchBox->setClearButtonEnabled(true);
    searchBox->setPlaceholderText(tr("Filter tag names"));
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::updateEntries);

    listView = new QListView(this);
    listView->setModel(model);
    listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listView->setContextMenuPolicy(Qt::ActionsContextMenu);

    QAction *openFiles = new QAction(tr("&Open"), this);
    connect(openFiles, &QAction::triggered, this, &MainWindow::openFiles);
    listView->addAction(openFiles);

    QAction *tagFiles = new QAction(tr("&Tag"), this);
    connect(tagFiles, &QAction::triggered, this, &MainWindow::tagFiles);
    listView->addAction(tagFiles);

    QAction *removeFiles = new QAction(tr("&Remove"), this);
    connect(removeFiles, &QAction::triggered, this, &MainWindow::removeFiles);
    listView->addAction(removeFiles);

    QGridLayout *mainLayout = new QGridLayout(centralWidget);
    mainLayout->addWidget(searchBox, 0, 0, 1, 1);
    mainLayout->addWidget(listView, 1, 0, 1, 3);
}

void MainWindow::addDirectory() {
    QStringList filenames;
    QString directory = QFileDialog::getExistingDirectory(this, "Add Directory");
    if (!directory.isEmpty()) {
        fs::path path = fs::path(directory.toStdString());
        for (auto& p: fs::directory_iterator(path)) {
            if (p.is_regular_file()) {
                filenames.append(p.path().string().c_str());
            }
        }
    }
    if (!filenames.isEmpty()) {
        QStringList new_entries = sql_insert_into(database, std::string(PRIMARY_KEY), filenames);
        if (!new_entries.isEmpty()) {
            entries += new_entries;
            model->setStringList(entries);
        }
    }
}

void MainWindow::addFiles() {
    QStringList filenames = QFileDialog::getOpenFileNames(this, "Add Files");
    if (!filenames.isEmpty()) {
        QStringList new_entries = sql_insert_into(database, std::string(PRIMARY_KEY), filenames);
        if (!new_entries.isEmpty()) {
            entries += new_entries;
            model->setStringList(entries);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    sqlite3_close(database);
}

void MainWindow::openFiles() {
    QStringList filenames = getSelectedFiles(listView);
    //TODO: don't hardcode this and actually use mime types
    std::string args = "mpv ";
    for (int i = 0; i < filenames.size(); ++i) {
        args += "\"" + filenames.at(i).toStdString() + "\" ";
    }
    popen(args.c_str(), "r");
}

void MainWindow::removeFiles() {
    QStringList filenames = getSelectedFiles(listView);
    if (!filenames.isEmpty()) {
        QStringList old_entries = sql_delete(database, std::string(PRIMARY_KEY), filenames);
        if (!old_entries.isEmpty()) {
            for (int i = 0; i < old_entries.size(); ++i) {
                entries.removeOne(old_entries.at(i));
            }
            model->setStringList(entries);
        }
    }
}

void MainWindow::tagFiles() {
    tagDialog = new QDialog(this);
    QLabel *tagLabel = new QLabel("Tags:", this);
    tagEdit = new QLineEdit(this);
    QPushButton *tagAdd = new QPushButton("Add Tags", this);
    QPushButton *tagDelete = new QPushButton("Delete Tags", this);

    connect(tagAdd, &QPushButton::released, this, [this]{ MainWindow::updateTags(true);});
    connect(tagDelete, &QPushButton::released, this, [this]{ MainWindow::updateTags(false);});

    QFormLayout *tagLayout = new QFormLayout();
    tagLayout->addRow(tagLabel, tagEdit);
    tagLayout->addRow(tagAdd, tagDelete);
    tagDialog->setLayout(tagLayout);

    tagDialog->setWindowTitle("Add or Delete Tags");
    tagDialog->show();
}

void MainWindow::updateEntries(const QString str) {
    char *err;
    QStringList filtered_entries;
    QStringList tags = splitTags(str.toStdString(), ',');
    std::string sql = std::string("SELECT ") + PRIMARY_KEY + " FROM " + TABLE + " WHERE ";

    QSet<QString> columns = sql_get_columns(database);
    for (int i = 0; i < tags.size(); ++i) {
        std::string tag = tags.at(i).toLower().toStdString();
        if (tags.at(i)[0] == '-') {
            tag = tags.at(i).mid(1).toLower().toStdString();
        }
        if (!columns.contains(tag.c_str())) {
            goto done;
        }
    }

    for (int i = 0; i < tags.size(); ++i) {
        std::string tag = tags.at(i).toStdString();
        std::string include = " NOT ";
        if (tags.at(i)[0] == '-') {
            include = "";
            tag = tags.at(i).mid(1).toStdString();
        }
        sql += "\"" + tag + "\" IS" + include + "NULL AND ";
    }
    sql.erase(sql.end() - 5, sql.end());
    sqlite3_exec(database, sql.c_str(), entry_callback, static_cast<void *>(&filtered_entries), &err);

done:
    model->setStringList(filtered_entries);
}

void MainWindow::updateTags(bool add) {
    QStringList filenames = getSelectedFiles(listView);
    QStringList tags = splitTags(tagEdit->text().toStdString(), ',');

    QStringList new_columns;
    QSet<QString> columns = sql_get_columns(database);
    for (int i = 0; i < tags.size(); ++i) {
        if (!columns.contains(tags[i].toLower())) {
            new_columns.append(tags[i].toLower());
        }
    }
    if (!columns.isEmpty()) {
        sql_add_columns(database, std::string(PRIMARY_KEY), new_columns);
    }
    if (!tags.isEmpty()) {
        sql_update_tags(database, std::string(PRIMARY_KEY), filenames, tags, add);
    }
    tagDialog->close();
}
