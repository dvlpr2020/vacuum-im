#include "mainwindowplugin.h"

#include <QApplication>
#include <QDesktopWidget>

MainWindowPlugin::MainWindowPlugin()
{
	FPluginManager = NULL;
	FOptionsManager = NULL;
	FTrayManager = NULL;

	FActivationChanged = QTime::currentTime();
#ifdef Q_WS_WIN
	FMainWindow = new MainWindow(new QWidget, Qt::Window|Qt::CustomizeWindowHint|Qt::WindowTitleHint|Qt::WindowCloseButtonHint);
#else
	FMainWindow = new MainWindow(NULL, Qt::Window|Qt::CustomizeWindowHint|Qt::WindowTitleHint|Qt::WindowCloseButtonHint);
#endif
	FMainWindow->installEventFilter(this);
	WidgetManager::setWindowSticky(FMainWindow,true);
}

MainWindowPlugin::~MainWindowPlugin()
{
	delete FMainWindow;
}

void MainWindowPlugin::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Main Window");
	APluginInfo->description = tr("Allows other modules to place their widgets in the main window");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Potapov S.A. aka Lion";
	APluginInfo->homePage = "http://www.vacuum-im.org";
}

bool MainWindowPlugin::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);
	FPluginManager = APluginManager;

	IPlugin *plugin = FPluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
	{
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());
		if (FOptionsManager)
		{
			connect(FOptionsManager->instance(), SIGNAL(profileRenamed(const QString &, const QString &)),
				SLOT(onProfileRenamed(const QString &, const QString &)));
		}
	}

	plugin = APluginManager->pluginInterface("ITrayManager").value(0,NULL);
	if (plugin)
	{
		FTrayManager = qobject_cast<ITrayManager *>(plugin->instance());
		if (FTrayManager)
		{
			connect(FTrayManager->instance(),SIGNAL(notifyActivated(int, QSystemTrayIcon::ActivationReason)),
				SLOT(onTrayNotifyActivated(int,QSystemTrayIcon::ActivationReason)));
		}
	}

	connect(Options::instance(),SIGNAL(optionsOpened()),SLOT(onOptionsOpened()));
	connect(Options::instance(),SIGNAL(optionsClosed()),SLOT(onOptionsClosed()));
	connect(FPluginManager->instance(),SIGNAL(shutdownStarted()),SLOT(onShutdownStarted()));
	connect(Shortcuts::instance(),SIGNAL(shortcutActivated(const QString, QWidget *)),SLOT(onShortcutActivated(const QString, QWidget *)));

	return true;
}

bool MainWindowPlugin::initObjects()
{
	Shortcuts::declareShortcut(SCT_GLOBAL_SHOWROSTER,tr("Show roster"),QKeySequence::UnknownKey,Shortcuts::GlobalShortcut);

	Shortcuts::declareGroup(SCTG_MAINWINDOW, tr("Main window"), SGO_MAINWINDOW);
	Shortcuts::declareShortcut(SCT_MAINWINDOW_CLOSEWINDOW,tr("Hide roster"),tr("Esc","Hide roster"));

	Action *action = new Action(this);
	action->setText(tr("Quit"));
	action->setIcon(RSR_STORAGE_MENUICONS,MNI_MAINWINDOW_QUIT);
	connect(action,SIGNAL(triggered()),FPluginManager->instance(),SLOT(quit()));
	FMainWindow->mainMenu()->addAction(action,AG_MMENU_MAINWINDOW,true);

	if (FTrayManager)
	{
		action = new Action(this);
		action->setText(tr("Show roster"));
		action->setIcon(RSR_STORAGE_MENUICONS,MNI_MAINWINDOW_SHOW_ROSTER);
		connect(action,SIGNAL(triggered(bool)),SLOT(onShowMainWindowByAction(bool)));
		FTrayManager->contextMenu()->addAction(action,AG_TMTM_MAINWINDOW,true);
	}

	Shortcuts::insertWidgetShortcut(SCT_MAINWINDOW_CLOSEWINDOW,FMainWindow);

	return true;
}

bool MainWindowPlugin::initSettings()
{
	Options::setDefaultValue(OPV_MAINWINDOW_SHOW,true);
	return true;
}

bool MainWindowPlugin::startPlugin()
{
	Shortcuts::setGlobalShortcut(SCT_GLOBAL_SHOWROSTER,true);

	updateTitle();
	return true;
}

IMainWindow *MainWindowPlugin::mainWindow() const
{
	return FMainWindow;
}

void MainWindowPlugin::updateTitle()
{
	if (FOptionsManager && FOptionsManager->isOpened())
		FMainWindow->setWindowTitle(CLIENT_NAME" - "+FOptionsManager->currentProfile());
	else
		FMainWindow->setWindowTitle(CLIENT_NAME);
}

void MainWindowPlugin::showMainWindow()
{
	if (!Options::isNull())
	{
		WidgetManager::showActivateRaiseWindow(FMainWindow);
		if (!FAligned)
		{
			FAligned = true;
			WidgetManager::alignWindow(FMainWindow,(Qt::Alignment)Options::node(OPV_MAINWINDOW_ALIGN).value().toInt());
		}
		correctWindowPosition();
	}
}

void MainWindowPlugin::correctWindowPosition()
{
	QRect windowRect = FMainWindow->geometry();
	QRect screenRect = qApp->desktop()->availableGeometry(FMainWindow);
	if (!screenRect.isEmpty() && !windowRect.isEmpty())
	{
		Qt::Alignment align = 0;
		if (windowRect.right() <= screenRect.left())
			align |= Qt::AlignLeft;
		else if (windowRect.left() >= screenRect.right())
			align |= Qt::AlignRight;
		if (windowRect.top() >= screenRect.bottom())
			align |= Qt::AlignBottom;
		else if (windowRect.bottom() <= screenRect.top())
			align |= Qt::AlignTop;
		WidgetManager::alignWindow(FMainWindow,align);
	}
}

bool MainWindowPlugin::eventFilter(QObject *AWatched, QEvent *AEvent)
{
	if (AWatched==FMainWindow && AEvent->type()==QEvent::ActivationChange)
		FActivationChanged = QTime::currentTime();
	return QObject::eventFilter(AWatched,AEvent);
}

void MainWindowPlugin::onOptionsOpened()
{
	FAligned = false;
	if (!FMainWindow->restoreGeometry(Options::fileValue("mainwindow.geometry").toByteArray()))
		FMainWindow->setGeometry(WidgetManager::alignGeometry(QSize(200,500),FMainWindow,Qt::AlignRight|Qt::AlignBottom));
	if (Options::node(OPV_MAINWINDOW_SHOW).value().toBool())
		showMainWindow();
	updateTitle();
}

void MainWindowPlugin::onOptionsClosed()
{
	Options::setFileValue(FMainWindow->saveGeometry(),"mainwindow.geometry");
	Options::node(OPV_MAINWINDOW_ALIGN).setValue((int)WidgetManager::windowAlignment(FMainWindow));
	updateTitle();
	FMainWindow->close();
}

void MainWindowPlugin::onShutdownStarted()
{
	Options::node(OPV_MAINWINDOW_SHOW).setValue(FMainWindow->isVisible());
}

void MainWindowPlugin::onProfileRenamed(const QString &AProfile, const QString &ANewName)
{
	Q_UNUSED(AProfile);
	Q_UNUSED(ANewName);
	updateTitle();
}

void MainWindowPlugin::onTrayNotifyActivated(int ANotifyId, QSystemTrayIcon::ActivationReason AReason)
{
	if (ANotifyId<=0 && AReason==QSystemTrayIcon::Trigger)
	{
		if (FMainWindow->isActive() || qAbs(FActivationChanged.msecsTo(QTime::currentTime())) < qApp->doubleClickInterval())
			FMainWindow->close();
		else
			showMainWindow();
	}
}

void MainWindowPlugin::onShowMainWindowByAction(bool)
{
	showMainWindow();
}

void MainWindowPlugin::onShortcutActivated(const QString &AId, QWidget *AWidget)
{
	if (AWidget==NULL && AId==SCT_GLOBAL_SHOWROSTER)
	{
		showMainWindow();
	}
	else if (AWidget==FMainWindow && AId==SCT_MAINWINDOW_CLOSEWINDOW)
	{
		FMainWindow->close();
	}
}

Q_EXPORT_PLUGIN2(plg_mainwindow, MainWindowPlugin)