#ifndef WINDOW_H
#define WINDOW_H

#include <QLineEdit>
#include <QMainWindow>
#include <sqlite3.h>

class MainWindow : public QMainWindow {
	public:
		explicit MainWindow(QWidget *parent = 0);
		QLineEdit *tagEdit;
		QListView *listView;
		QStringList entries;
		QStringListModel *model;
		QDialog *tagDialog;
		sqlite3 *database;
	private slots:
		void addDirectory();
		void addFiles();
		void closeEvent(QCloseEvent *event);
		void openFiles();
		void removeFiles();
		void tagFiles();
		void updateEntries(const QString str);
		void updateTags(bool add);
};

#endif
