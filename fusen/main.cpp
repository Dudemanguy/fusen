#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <pwd.h>
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QListView>
#include <QMenuBar>
#include <QPushButton>
#include <QSet>
#include <QStringListModel>
#include <yaml-cpp/yaml.h>
#include <unistd.h>

#include "mainwindow.h"

#define TABLE "master"
#define PRIMARY_KEY "path"

namespace fs = std::filesystem;
static fs::path getUserFile(const char *type);

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");

    MainWindow window;
    window.show();

    return app.exec();
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

static int entry_callback(void *data, int argc, char **argv, char **azColName) {
    QSet<QString> *entries = static_cast<QSet<QString> *>(data);
    for (int i = 0; i < argc; i++) {
        entries->insert(argv[i]);
    }
    return 0;
}

static QStringList build_entries(sqlite3 *database) {
    QSet<QString> entries;
    std::string sql = std::string("SELECT ") + PRIMARY_KEY + std::string(" FROM ") + TABLE;
    char *err;
    if (sqlite3_exec(database, sql.c_str(), entry_callback, static_cast<void *>(&entries), &err)) {
        std::cerr << "Error while trying to read table: " << err << std::endl;
    }
    QStringList list(entries.begin(), entries.end());
    return list;
}

static sqlite3 *connectDatabase() {
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

static QStringList getSelectedFiles(QListView *listView) {
    QModelIndexList indexes = listView->selectionModel()->selectedIndexes();
    QStringList list;
    foreach (const QModelIndex &index, indexes) {
        list << index.data(Qt::DisplayRole).toString();
    }
    return list;
}

static fs::path getUserFile(const char *type) {
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

static void initializeSettings(mainSettings *settings) {
    fs::path file = getUserFile("settings");
    if (!fs::exists(file)) {
        fs::create_directories(file.parent_path());
        return;
    }

    YAML::Node yaml = YAML::LoadFile(file.string().c_str());
    if (yaml["clearTagsOnImport"]) {
        settings->clearTags->setChecked(yaml["clearTagsOnImport"].as<bool>());
    }
    if (yaml["defaultApplicationPath"]) {
        settings->defaultApplicationPath = yaml["defaultApplicationPath"].as<std::string>();
    } else {
        settings->defaultApplicationPath = "mpv";
    }
}

std::string sanitize_tags(std::string str) {
    // Replace some special characters and other nonsense for sanity
    // An sql injection is probably still possible but whatever don't make drop table a tag
    std::replace(str.begin(), str.end(), ' ', '_');
    std::replace(str.begin(), str.end(), '\'', '_');
    std::replace(str.begin(), str.end(), '"', '_');
    return str;
}

static void saveSettings(mainSettings *settings) {
    YAML::Node yaml;
    yaml["clearTagsOnImport"] = settings->clearTags->isChecked();
    yaml["defaultApplicationPath"] = settings->defaultApplicationPath.c_str();

    fs::path file = getUserFile("settings");
    std::ofstream fout(file.string().c_str());
    fout << yaml;
    fout.close();
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

static void sql_clear_tags(sqlite3 *database, QSet<QString> columns, QStringList filenames) {
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
        sql += primary_key + " = '" + filenames.at(i).toStdString() + "' OR ";
    }
    sql.erase(sql.end() - 3, sql.end());
    if (sqlite3_exec(database, sql.c_str(), NULL, 0, &err)) {
        std::cerr << "Error while clearing tags: " << err << std::endl;
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
    settings = new mainSettings;
    database = connectDatabase();

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *addFiles = new QAction(tr("&Add Files"), this);
    connect(addFiles, &QAction::triggered, this, &MainWindow::addFiles);
    fileMenu->addAction(addFiles);

    QAction *addDirectory = new QAction(tr("&Add Directory"), this);
    connect(addDirectory, &QAction::triggered, this, [this]{MainWindow::addDirectory(false);});
    fileMenu->addAction(addDirectory);

    QAction *addRecursiveDirectory = new QAction(tr("&Add Directory Recursively"), this);
    connect(addRecursiveDirectory, &QAction::triggered, this, [this]{MainWindow::addDirectory(true);});
    fileMenu->addAction(addRecursiveDirectory);

    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));

    clearTags = new QAction(tr("&Clear Existing Tags on Import"), this);
    clearTags->setCheckable(true);
    settingsMenu->addAction(clearTags);
    settings->clearTags = clearTags;

    QAction *defaultOpen = new QAction(tr("&Default Application to Open Files"), this);
    connect(defaultOpen, &QAction::triggered, this, &MainWindow::defaultApplicationOpen);
    settingsMenu->addAction(defaultOpen);

    initializeSettings(settings);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    entries = build_entries(database);
    model = new QStringListModel(this);
    entries.sort();
    model->setStringList(entries);

    searchBox = new QLineEdit(this);
    searchBox->setClearButtonEnabled(true);
    searchBox->setPlaceholderText(tr("Search"));
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::updateEntries);

    searchNames = new QCheckBox("Filter by name", this);
    connect(searchNames, &QCheckBox::stateChanged, this, &MainWindow::updateNameCheck);

    QPushButton *importTags = new QPushButton("Import", this);
    connect(importTags, &QPushButton::released, this, &MainWindow::importTags);

    listView = new QListView(this);
    listView->setModel(model);
    listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listView->setContextMenuPolicy(Qt::ActionsContextMenu);

    QAction *openFiles = new QAction(tr("&Open"), this);
    connect(openFiles, &QAction::triggered, this, [this]{MainWindow::openFiles(true);});
    listView->addAction(openFiles);

    QAction *openFilesWith = new QAction(tr("&Open With"), this);
    connect(openFilesWith, &QAction::triggered, this, &MainWindow::openFilesWith);
    listView->addAction(openFilesWith);

    QAction *tagFiles = new QAction(tr("&Tag"), this);
    connect(tagFiles, &QAction::triggered, this, &MainWindow::tagFiles);
    listView->addAction(tagFiles);

    QAction *removeFiles = new QAction(tr("&Remove"), this);
    connect(removeFiles, &QAction::triggered, this, &MainWindow::removeFiles);
    listView->addAction(removeFiles);

    QGridLayout *mainLayout = new QGridLayout(centralWidget);
    mainLayout->addWidget(searchBox, 0, 0, 1, 1);
    mainLayout->addWidget(searchNames, 0, 1, Qt::AlignLeft);
    mainLayout->addWidget(importTags, 0, 2, Qt::AlignLeft);
    mainLayout->addWidget(listView, 1, 0, 1, 3);
}

void MainWindow::addDirectory(bool recursive) {
    char *err;
    std::string sql = std::string("SELECT ") + PRIMARY_KEY + " FROM " + TABLE;
    QSet<QString> existing_files;
    if (sqlite3_exec(database, sql.c_str(), entry_callback, static_cast<void *>(&existing_files), &err)) {
        std::cerr << "Error while reading database: " << err << std::endl;
        return;
    }

    QStringList filenames;
    QString directory = QFileDialog::getExistingDirectory(this, "Add Directory");
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
    if (!filenames.isEmpty()) {
        QStringList new_entries = sql_insert_into(database, std::string(PRIMARY_KEY), filenames);
        if (!new_entries.isEmpty()) {
            entries += new_entries;
            entries.sort();
            model->setStringList(entries);
        }
    }
}

void MainWindow::addFiles() {
    char *err;
    std::string sql = std::string("SELECT ") + PRIMARY_KEY + " FROM " + TABLE;
    QSet<QString> existing_files;
    if (sqlite3_exec(database, sql.c_str(), entry_callback, static_cast<void *>(&existing_files), &err)) {
        std::cerr << "Error while reading database: " << err << std::endl;
        return;
    }

    QStringList filenames = QFileDialog::getOpenFileNames(this, "Add Files");
    QStringList filtered_filenames;
    for (int i = 0; i < filenames.size(); ++i) {
        if (!existing_files.contains(filenames.at(i))) {
            filenames.append(filenames.at(i));
        }
    }

    if (!filtered_filenames.isEmpty()) {
        QStringList new_entries = sql_insert_into(database, std::string(PRIMARY_KEY), filtered_filenames);
        if (!new_entries.isEmpty()) {
            entries += new_entries;
            entries.sort();
            model->setStringList(entries);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    sqlite3_close(database);
    saveSettings(settings);
    delete settings;
}

void MainWindow::defaultApplicationOpen() {
    defaultOpen = new QDialog(this);
    QLabel *label = new QLabel("Default Application Path:", this);
    defaultOpenWith = new QLineEdit(this);
    QPushButton *ok = new QPushButton("OK", this);
    QPushButton *cancel = new QPushButton("Cancel", this);

    connect(ok, &QPushButton::released, this, [this]{ MainWindow::updateApplication(true);});
    connect(cancel, &QPushButton::released, this, [this]{ MainWindow::updateApplication(false);});

    QFormLayout *layout = new QFormLayout();
    layout->addRow(label, defaultOpenWith);
    layout->addRow(ok, cancel);
    defaultOpen->setLayout(layout);

    defaultOpen->setWindowTitle("Default Application");
    defaultOpen->show();
}

void MainWindow::importTags() {
    QFileDialog *fileDialog = new QFileDialog;
    QString filename = fileDialog->getOpenFileName(this, "Import Tags", "", "YAML (*.yaml *.yml)");
    if (!filename.isEmpty()) {
        QSet<QString> columns = sql_get_columns(database);
        YAML::Node node = YAML::LoadFile(filename.toStdString().c_str());
        for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
            QStringList filenames;
            QStringList new_columns;
            QStringList tags;
            filenames.append(it->first.as<std::string>().c_str());
            YAML::Node value = it->second;
            switch (value.Type()) {
            case YAML::NodeType::Sequence:
                for (size_t i = 0; i < value.size(); ++i) {
                    QString tag = sanitize_tags(it->second[i].as<std::string>()).c_str();
                    if (!columns.contains(tag.toLower())) {
                        columns.insert(tag.toLower());
                        new_columns.append(tag.toLower());
                    }
                    tags.append(tag);
                }
                break;
            default:
                break;
            }
            if (!new_columns.isEmpty()) {
                sql_add_columns(database, std::string(PRIMARY_KEY), new_columns);
            }
            if (settings->clearTags->isChecked()) {
                sql_clear_tags(database, columns, filenames);
            }
            sql_update_tags(database, std::string(PRIMARY_KEY), filenames, tags, true);
        }
    }
}

void MainWindow::openFiles(bool defaultApplication) {
    QStringList filenames = getSelectedFiles(listView);
    //TODO: use mime types somehow
    std::string args = defaultApplication ? settings->defaultApplicationPath : openWithEntry->text().toStdString();
    args += " ";
    for (int i = 0; i < filenames.size(); ++i) {
        args += "\"" + filenames.at(i).toStdString() + "\" ";
    }
    popen(args.c_str(), "r");
}

void MainWindow::openFilesWith() {
    openWith = new QDialog(this);
    QLabel *label = new QLabel("Open With:", this);
    openWithEntry = new QLineEdit(this);
    QPushButton *ok = new QPushButton("OK", this);
    QPushButton *cancel = new QPushButton("Cancel", this);

    connect(ok, &QPushButton::released, this, [this]{ openWith->close(); MainWindow::openFiles(false);});
    connect(cancel, &QPushButton::released, this, [this]{ openWith->close();});

    QFormLayout *layout = new QFormLayout();
    layout->addRow(label, openWithEntry);
    layout->addRow(ok, cancel);
    openWith->setLayout(layout);

    openWith->setWindowTitle("Open With");
    openWith->show();
}


void MainWindow::removeFiles() {
    QStringList filenames = getSelectedFiles(listView);
    if (!filenames.isEmpty()) {
        QStringList old_entries = sql_delete(database, std::string(PRIMARY_KEY), filenames);
        if (!old_entries.isEmpty()) {
            for (int i = 0; i < old_entries.size(); ++i) {
                entries.removeOne(old_entries.at(i));
            }
            entries.sort();
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

void MainWindow::updateApplication(bool update) {
    if (update) {
        settings->defaultApplicationPath = defaultOpenWith->text().toStdString();
    }
    defaultOpen->close();
}

void MainWindow::updateEntries(const QString str) {
    char *err;
    QSet<QString> filtered_entries;
    QSet<QString> columns;
    QStringList tags;
    std::string sql;

    if (searchNames->isChecked()) {
        entries = build_entries(database);
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).toLower().contains(str.toLower())) {
                filtered_entries.insert(entries.at(i));
            }
        }
        goto done;
    }

    columns = sql_get_columns(database);
    tags = splitTags(str.toStdString(), ',');
    for (int i = 0; i < tags.size(); ++i) {
        std::string tag = tags.at(i).toLower().toStdString();
        if (tags.at(i)[0] == '-') {
            tag = tags.at(i).mid(1).toLower().toStdString();
        }
        if (!columns.contains(tag.c_str())) {
            goto done;
        }
    }

    sql = std::string("SELECT ") + PRIMARY_KEY + " FROM " + TABLE + " WHERE ";
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
    QStringList filtered_list(filtered_entries.begin(), filtered_entries.end());
    entries = filtered_list;
    entries.sort();
    model->setStringList(entries);
}

void MainWindow::updateNameCheck(bool checked) {
    MainWindow::updateEntries(searchBox->text());
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
