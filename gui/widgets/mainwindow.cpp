#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <localsettings.h>
#include <syncedsettings.h>

#include <QtMvvmCore/Binding>

MainWindow::MainWindow(QtMvvm::ViewModel *viewModel, QWidget *parent) :
	QMainWindow{parent},
	_viewModel{static_cast<MainViewModel*>(viewModel)},
	_ui{new Ui::MainWindow{}},
	_proxyModel{new ReminderProxyModel{this}}
{
	_ui->setupUi(this);
	setCentralWidget(_ui->treeView);

	connect(_viewModel, &MainViewModel::select,
			this, &MainWindow::select);
	connect(_ui->action_Close, &QAction::triggered,
			this, &MainWindow::close);
	connect(_ui->action_Settings, &QAction::triggered,
			_viewModel, &MainViewModel::showSettings);
	connect(_ui->actionS_ynchronization, &QAction::triggered,
			_viewModel, &MainViewModel::showSync);
	connect(_ui->action_About, &QAction::triggered,
			_viewModel, &MainViewModel::showAbout);
	connect(_ui->action_Add_Reminder, &QAction::triggered,
			_viewModel, &MainViewModel::addReminder);

	auto sep1 = new QAction(this);
	sep1->setSeparator(true);
	auto sep2 = new QAction(this);
	sep2->setSeparator(true);
	_ui->treeView->addActions({
								  _ui->action_Add_Reminder,
								  _ui->action_Delete_Reminder,
								  sep1,
								  _ui->action_Complete_Reminder,
								  _ui->action_Snooze_Reminder,
								  sep2,
								  _ui->actionOpen_URLs,
							  });

	_proxyModel->setSourceModel(_viewModel->sortedModel());
	_ui->treeView->setModel(_proxyModel);
	_ui->treeView->sortByColumn(1, Qt::AscendingOrder);

	_ui->treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	_ui->treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

	connect(_ui->treeView->selectionModel(), &QItemSelectionModel::currentChanged,
			this, &MainWindow::updateCurrent);
	connect(_viewModel->reminderModel(), &QtDataSync::DataStoreModel::dataChanged,
			this, [this](){
		updateCurrent(_ui->treeView->currentIndex());
	});

	//restore window geom
	if(LocalSettings::instance()->gui.mainwindow.geom.isSet())
		restoreGeometry(LocalSettings::instance()->gui.mainwindow.geom);
	if(LocalSettings::instance()->gui.mainwindow.state.isSet())
		restoreState(LocalSettings::instance()->gui.mainwindow.state);
	if(LocalSettings::instance()->gui.mainwindow.header.isSet())
		_ui->treeView->header()->restoreState(LocalSettings::instance()->gui.mainwindow.header);
}

MainWindow::~MainWindow()
{
	LocalSettings::instance()->gui.mainwindow.geom = saveGeometry();
	LocalSettings::instance()->gui.mainwindow.state = saveState();
	LocalSettings::instance()->gui.mainwindow.header = _ui->treeView->header()->saveState();

	delete _ui;
}

void MainWindow::select(int row)
{
	auto index = _viewModel->reminderModel()->index(row);
	index = _viewModel->sortedModel()->mapFromSource(index);
	index = _proxyModel->mapFromSource(index);
	_ui->treeView->setCurrentIndex(index);
}

void MainWindow::on_action_Complete_Reminder_triggered()
{
	_viewModel->completeReminder(reminderAt(_ui->treeView->currentIndex()).id());
}

void MainWindow::on_action_Delete_Reminder_triggered()
{
	_viewModel->deleteReminder(reminderAt(_ui->treeView->currentIndex()).id());
}

void MainWindow::on_action_Snooze_Reminder_triggered()
{
	_viewModel->snoozeReminder(reminderAt(_ui->treeView->currentIndex()).id());
}

void MainWindow::on_actionOpen_URLs_triggered()
{
	reminderAt(_ui->treeView->currentIndex()).openUrls();
}

void MainWindow::on_treeView_activated(const QModelIndex &index)
{
	_viewModel->snoozeReminder(reminderAt(index).id());
}

void MainWindow::updateCurrent(const QModelIndex &index)
{
	if(index.isValid()) {
		auto reminder = reminderAt(index);
		_ui->actionOpen_URLs->setVisible(reminder.hasUrls());
	} else {
		_ui->actionOpen_URLs->setVisible(false);
	}
}

Reminder MainWindow::reminderAt(const QModelIndex &sIndex)
{
	if(!sIndex.isValid()) //NOTE use special property from datasync once created
		return {};

	auto index = _proxyModel->mapToSource(sIndex);
	if(!index.isValid())
		return {};

	index = _viewModel->sortedModel()->mapToSource(index);
	if(!index.isValid())
		return {};

	return _viewModel->reminderModel()->object<Reminder>(index);
}



ReminderProxyModel::ReminderProxyModel(QObject *parent) :
	QIdentityProxyModel{parent}
{}

QVariant ReminderProxyModel::data(const QModelIndex &index, int role) const
{
	auto data = QIdentityProxyModel::data(index, role);
	if(!data.isValid())
		return {};

	auto format = static_cast<QLocale::FormatType>(SyncedSettings::instance()->gui.dateformat.get());
	switch (index.column()) {
	case 0:
		if(role == Qt::DecorationRole) {
			if(data.toBool())
				return QIcon::fromTheme(QStringLiteral("emblem-important-symbolic"), QIcon{QStringLiteral(":/icons/important.ico")});
			else
				return QIcon(QStringLiteral(":/icons/empty.ico"));
		} else if(role == Qt::ToolTipRole) {
			auto important = QIdentityProxyModel::data(index, Qt::DecorationRole).toBool();
			if(important)
				return data.toString().append(tr("<br/><i>This is an important reminder</i>"));
		}
		break;
	case 1:
		if(role == Qt::DecorationRole) {
			switch(data.toInt()) {
			case Reminder::Normal:
				return QIcon(QStringLiteral(":/icons/empty.ico"));
			case Reminder::NormalRepeating:
				return QIcon::fromTheme(QStringLiteral("media-playlist-repeat"), QIcon(QStringLiteral(":/icons/loop.ico")));
			case Reminder::Snoozed:
				return QIcon::fromTheme(QStringLiteral("alarm-symbolic"), QIcon(QStringLiteral(":/icons/snooze.ico")));
			case Reminder::Triggered:
				return QIcon::fromTheme(QStringLiteral("view-calendar-upcoming-events"), QIcon(QStringLiteral(":/icons/trigger.ico")));
			default:
				break;
			}
		} else if(role == Qt::ToolTipRole) {
			auto dateTime = QIdentityProxyModel::data(index, Qt::DisplayRole);
			auto baseStr = QLocale().toString(dateTime.toDateTime(), QLocale::LongFormat);
			switch(data.toInt()) {
			case Reminder::Normal:
				return baseStr;
			case Reminder::NormalRepeating:
				return baseStr.append(tr("\nReminder will repeatedly trigger, not only once"));
			case Reminder::Snoozed:
				return baseStr.append(tr("\nReminder has been snoozed until the displayed time"));
			case Reminder::Triggered:
				return baseStr.append(tr("\nReminder has been triggered and needs a reaction!"));
			default:
				break;
			}
		} else if(role == Qt::DisplayRole)
			return QLocale().toString(data.toDateTime(), format);
		break;
	default:
		break;
	}

	return data;
}
