#include "statuschanger.h"

#include <QTimer>
#include <QToolButton>

#define MAX_TEMP_STATUS_ID                  -10

#define SVN_LAST_ONLINE_MAIN_STATUS         "lastOnlineMainStatus"
#define SVN_MAIN_STATUS_ID                  "mainStatus"
#define SVN_STATUS                          "status[]"
#define SVN_STATUS_CODE                     "status[]:code"
#define SVN_STATUS_NAME                     "status[]:name"
#define SVN_STATUS_SHOW                     "status[]:show"
#define SVN_STATUS_TEXT                     "status[]:text"
#define SVN_STATUS_PRIORITY                 "status[]:priority"
#define SVN_STATUS_ICONSET                  "status[]:iconset"
#define SVN_STATUS_ICON_NAME                "status[]:iconName"

#define NOTIFICATOR_ID                      "StatusChanger"

StatusChanger::StatusChanger()
{
  FConnectingLabel = RLID_NULL;
  FPresencePlugin = NULL;
  FRosterPlugin = NULL;
  FMainWindowPlugin = NULL;
  FRostersView = NULL;
  FRostersViewPlugin = NULL;
  FRostersModel = NULL;
  FTrayManager = NULL;
  FSettingsPlugin = NULL;
  FEditStatusAction = NULL;
  FMainMenu = NULL;
  FSettingStatusToPresence = NULL;
  FAccountManager = NULL;
  FStatusIcons = NULL;
  FNotifications = NULL;
}

StatusChanger::~StatusChanger()
{
  if (!FEditStatusDialog.isNull())
    FEditStatusDialog->reject();
  FStatusItems.remove(MAIN_STATUS_ID);
  qDeleteAll(FStatusItems);
  if (FMainMenu)
    delete FMainMenu;
}

//IPlugin
void StatusChanger::pluginInfo(IPluginInfo *APluginInfo)
{
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->description = tr("Managing and change status");
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->name = tr("Status Changer"); 
  APluginInfo->uid = STATUSCHANGER_UUID;
  APluginInfo->version = "0.1";
  APluginInfo->dependences.append(PRESENCE_UUID);
}

bool StatusChanger::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  IPlugin *plugin = APluginManager->getPlugins("IPresencePlugin").value(0,NULL);
  if (plugin)
  {
    FPresencePlugin = qobject_cast<IPresencePlugin *>(plugin->instance());
    if (FPresencePlugin)
    {
      connect(FPresencePlugin->instance(),SIGNAL(presenceAdded(IPresence *)),
        SLOT(onPresenceAdded(IPresence *)));
      connect(FPresencePlugin->instance(),SIGNAL(presenceChanged(IPresence *, int, const QString &, int)),
        SLOT(onPresenceChanged(IPresence *, int, const QString &, int)));
      connect(FPresencePlugin->instance(),SIGNAL(presenceRemoved(IPresence *)),
        SLOT(onPresenceRemoved(IPresence *)));
    }
  }

  plugin = APluginManager->getPlugins("IRosterPlugin").value(0,NULL);
  if (plugin)
  {
    FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());
    if (FRosterPlugin)
    {
      connect(FRosterPlugin->instance(),SIGNAL(rosterOpened(IRoster *)),SLOT(onRosterOpened(IRoster *)));
      connect(FRosterPlugin->instance(),SIGNAL(rosterClosed(IRoster *)),SLOT(onRosterClosed(IRoster *)));
    }
  }

  plugin = APluginManager->getPlugins("IMainWindowPlugin").value(0,NULL);
  if (plugin)
    FMainWindowPlugin = qobject_cast<IMainWindowPlugin *>(plugin->instance());

  plugin = APluginManager->getPlugins("IRostersViewPlugin").value(0,NULL);
  if (plugin)
    FRostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());
  
  plugin = APluginManager->getPlugins("IRostersModel").value(0,NULL);
  if (plugin)
  {
    FRostersModel = qobject_cast<IRostersModel *>(plugin->instance());
    if (FRostersModel)
    {
      connect(FRostersModel->instance(),SIGNAL(streamJidChanged(const Jid &, const Jid &)),
        SLOT(onStreamJidChanged(const Jid &, const Jid &)));
    }
  }

  plugin = APluginManager->getPlugins("IAccountManager").value(0,NULL);
  if (plugin)
  {
    FAccountManager = qobject_cast<IAccountManager *>(plugin->instance());
    if (FAccountManager)
    {
      connect(FAccountManager->instance(),SIGNAL(optionsAccepted()),SLOT(onOptionsAccepted()));
      connect(FAccountManager->instance(),SIGNAL(optionsRejected()),SLOT(onOptionsRejected()));
    }
  }

  plugin = APluginManager->getPlugins("ITrayManager").value(0,NULL);
  if (plugin)
    FTrayManager = qobject_cast<ITrayManager *>(plugin->instance());
  
  plugin = APluginManager->getPlugins("ISettingsPlugin").value(0,NULL);
  if (plugin)
  {
    FSettingsPlugin = qobject_cast<ISettingsPlugin *>(plugin->instance());
    if (FSettingsPlugin)
    {
      connect(FSettingsPlugin->instance(),SIGNAL(settingsOpened()),SLOT(onSettingsOpened()));
      connect(FSettingsPlugin->instance(),SIGNAL(settingsClosed()),SLOT(onSettingsClosed()));
      connect(FSettingsPlugin->instance(),SIGNAL(optionsDialogClosed()),SLOT(onOptionsDialogClosed()));
    }
  }

  plugin = APluginManager->getPlugins("IStatusIcons").value(0,NULL);
  if (plugin)
  {
    FStatusIcons = qobject_cast<IStatusIcons *>(plugin->instance());
  }

  plugin = APluginManager->getPlugins("INotifications").value(0,NULL);
  if (plugin)
  {
    FNotifications = qobject_cast<INotifications *>(plugin->instance());
    if (FNotifications)
    {
      connect(FNotifications->instance(),SIGNAL(notificationActivated(int)), SLOT(onNotificationActivated(int)));
    }
  }

  return FPresencePlugin!=NULL;
}

bool StatusChanger::initObjects()
{
  FMainMenu = new Menu;

  FEditStatusAction = new Action(FMainMenu);
  FEditStatusAction->setText(tr("Edit presence statuses"));
  FEditStatusAction->setIcon(RSR_STORAGE_MENUICONS,MNI_SCHANGER_EDIT_STATUSES);
  connect(FEditStatusAction,SIGNAL(triggered(bool)), SLOT(onEditStatusAction(bool)));
  FMainMenu->addAction(FEditStatusAction,AG_STATUSCHANGER_STATUSMENU_ACTIONS,false);
  
  createDefaultStatus();
  setMainStatusId(STATUS_OFFLINE);

  if (FSettingsPlugin)
    FSettingsPlugin->insertOptionsHolder(this);

  if (FMainWindowPlugin)
  {
    ToolBarChanger *changer = FMainWindowPlugin->mainWindow()->bottomToolBarChanger();
    QToolButton *button = changer->addToolButton(FMainMenu->menuAction(),AG_DEFAULT,false);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
  }

  if (FRostersViewPlugin)
  {
    FRostersView = FRostersViewPlugin->rostersView();
    FConnectingLabel = FRostersView->createIndexLabel(RLO_CONNECTING,IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_SCHANGER_CONNECTING),IRostersView::LabelBlink);
    connect(FRostersView,SIGNAL(contextMenu(IRosterIndex *, Menu *)),
      SLOT(onRostersViewContextMenu(IRosterIndex *, Menu *)));
  }

  if (FTrayManager)
  {
    FTrayManager->addAction(FMainMenu->menuAction(),AG_STATUSCHANGER_TRAY,true);
  }

  if (FStatusIcons)
  {
    connect(FStatusIcons->instance(),SIGNAL(defaultIconsChanged()),
      SLOT(onDefaultStatusIconsChanged()));
  }

  if (FNotifications)
  {
    uchar kindMask = INotification::PopupWindow|INotification::PlaySound;
    FNotifications->insertNotificator(NOTIFICATOR_ID,tr("Connection errors"),kindMask,kindMask);
  }

  return true;
}

bool StatusChanger::startPlugin()
{
  foreach(IPresence *presence, FStreamStatus.keys())
  {
    IAccount *account = FAccountManager!=NULL ? FAccountManager->accountByStream(presence->streamJid()) : NULL;
    if (account!=NULL && account->value(AVN_AUTOCONNECT,false).toBool())
    {
      int statusId = FStreamMainStatus.contains(presence) ? MAIN_STATUS_ID : account->value(AVN_LAST_ONLINE_STATUS, STATUS_ONLINE).toInt();
      if (!FStatusItems.contains(statusId))
        statusId = STATUS_ONLINE;
      setStatus(statusId,presence->streamJid());
    }
  }
  updateMainMenu();
  return true;
}

//IOptionsHolder
QWidget *StatusChanger::optionsWidget(const QString &ANode, int &AOrder)
{
  QStringList nodeTree = ANode.split("::",QString::SkipEmptyParts);
  if (nodeTree.count()==2 && nodeTree.at(0)==ON_ACCOUNTS)
  {
    AOrder = OO_ACCOUNT_STATUS;
    QString accountId = nodeTree.at(1);
    AccountOptionsWidget *widget = new AccountOptionsWidget(accountId);
    IAccount *account = FAccountManager!=NULL ? FAccountManager->accountById(accountId) : NULL;
    if (account)
    {
      widget->setAutoConnect(account->value(AVN_AUTOCONNECT,false).toBool());
      widget->setAutoReconnect(account->value(AVN_AUTORECONNECT,true).toBool());
    }
    else
    {
      widget->setAutoConnect(false);
      widget->setAutoReconnect(true);
    }
    FAccountOptionsById.insert(accountId,widget);
    return widget;
  }
  return NULL;
}

//IStatusChanger
void StatusChanger::setStatus(int AStatusId, const Jid &AStreamJid)
{
  if (FStatusItems.contains(AStatusId)) 
  {
    bool changeMainStatus = !AStreamJid.isValid() && AStatusId != MAIN_STATUS_ID;

    if (changeMainStatus)
      setMainStatusId(AStatusId);

    StatusItem *status = FStatusItems.value(AStatusId);
    QHash<IPresence *, int>::const_iterator it = FStreamStatus.constBegin();
    while (it != FStreamStatus.constEnd())
    {
      IPresence *presence = it.key();
      
      bool setStatusToPresence = presence->streamJid() == AStreamJid;
      setStatusToPresence |= FStreamMenu.count() == 1;
      setStatusToPresence |= changeMainStatus && FStreamMainStatus.contains(presence);
      setStatusToPresence |= !AStreamJid.isValid() && AStatusId == MAIN_STATUS_ID;
      setStatusToPresence |= changeMainStatus && status->show == IPresence::Offline;
      if (setStatusToPresence)
      {
        if (AStatusId == MAIN_STATUS_ID)
          FStreamMainStatus += presence;
        else if (!changeMainStatus)
          FStreamMainStatus -= presence;

        statusAboutToBeSeted(status->code,presence->streamJid());
        
        FSettingStatusToPresence = presence;
        if (!presence->setPresence((IPresence::Show)status->show,status->text,status->priority))
        {
          FSettingStatusToPresence = NULL;
          if (status->show != IPresence::Offline && !presence->xmppStream()->isOpen())
          {
            insertConnectingLabel(presence);
            FStreamWaitStatus.insert(presence,FStreamMainStatus.contains(presence) ? MAIN_STATUS_ID : status->code);
            presence->xmppStream()->open();
          }
        }
        else
        {
          FSettingStatusToPresence = NULL;
          setStreamStatusId(presence,changeMainStatus ? MAIN_STATUS_ID : AStatusId);

          if (status->show == IPresence::Offline)
            presence->xmppStream()->close();
          
          if (!FStreamMainStatus.contains(presence))
            FStreamLastStatus.insert(presence->streamJid(),AStatusId);
          
          updateStreamMenu(presence);
          updateMainMenu();
          emit statusSeted(status->code, presence->streamJid());
        }
      }
      it++;
    }
  }
}

int StatusChanger::streamStatus(const Jid &AStreamJid) const
{
  QHash<IPresence *, int>::const_iterator it = FStreamStatus.constBegin();
  while (it!=FStreamStatus.constEnd())
  {
    if (it.key()->streamJid() == AStreamJid)
      return it.value();
    it++;
  }
  return NULL_STATUS_ID;
}

Menu *StatusChanger::streamMenu(const Jid &AStreamJid) const
{
  QHash<IPresence *, Menu *>::const_iterator it = FStreamMenu.constBegin();
  while (it!=FStreamMenu.constEnd())
  {
    if (it.key()->streamJid() == AStreamJid)
      return it.value();
    it++;
  }
  return NULL;
}

int StatusChanger::addStatusItem(const QString &AName, int AShow, const QString &AText, int APriority)
{
  int statusId = statusByName(AName);
  if (statusId == 0 && !AName.isEmpty())
  {
    do {
      statusId = qrand() + (qrand() << 16);
    } while(statusId <= MAX_STANDART_STATUS_ID || FStatusItems.contains(statusId));
    StatusItem *status = new StatusItem;
    status->code = statusId;
    status->name = AName;
    status->show = AShow;
    status->text = AText;
    status->priority = APriority;
    FStatusItems.insert(statusId,status);
    createStatusActions(statusId);
    emit statusItemAdded(statusId);
  }
  else if (statusId > 0)
    updateStatusItem(statusId,AName,AShow,AText,APriority);

  return statusId;
}

void StatusChanger::updateStatusItem(int AStatusId, const QString &AName, int AShow, 
                                     const QString &AText, int APriority)
{
  if (FStatusItems.contains(AStatusId))
  {
    StatusItem *status = FStatusItems.value(AStatusId);
    if (status->name == AName || statusByName(AName) == NULL_STATUS_ID)
    {
      status->name = AName;
      status->show = AShow;
      status->text = AText;
      status->priority = APriority;
      updateStatusActions(AStatusId);
      emit statusItemChanged(AStatusId);
      resendUpdatedStatus(AStatusId);
    }
  }
}

void StatusChanger::removeStatusItem(int AStatusId)
{
  if (AStatusId > MAX_STANDART_STATUS_ID && FStatusItems.contains(AStatusId) && !activeStatusItems().contains(AStatusId))
  {
    emit statusItemRemoved(AStatusId);
    removeStatusActions(AStatusId);
    delete FStatusItems.take(AStatusId);;
  }
}

QString StatusChanger::statusItemName(int AStatusId) const
{
  if (FStatusItems.contains(AStatusId))
    return FStatusItems.value(AStatusId)->name;
  return QString();
}

int StatusChanger::statusItemShow(int AStatusId) const
{
  if (FStatusItems.contains(AStatusId))
    return FStatusItems.value(AStatusId)->show;
  return -1;
}

QString StatusChanger::statusItemText(int AStatusId) const
{
  if (FStatusItems.contains(AStatusId))
    return FStatusItems.value(AStatusId)->text;
  return QString();
}

int StatusChanger::statusItemPriority(int AStatusId) const
{
  if (FStatusItems.contains(AStatusId))
    return FStatusItems.value(AStatusId)->priority;
  return 0;
}

QIcon StatusChanger::statusItemIcon(int AStatusId) const
{
  if (FStatusItems.contains(AStatusId))
    return FStatusItems.value(AStatusId)->icon;
  return QIcon();
}

QList<int> StatusChanger::activeStatusItems() const
{
  QList<int> active;
  foreach (int statusId, FStreamStatus)
    active.append(statusId == MAIN_STATUS_ID ? FStatusItems.value(MAIN_STATUS_ID)->code : statusId);
  return active;
}

void StatusChanger::setStatusItemIcon(int AStatusId, const QIcon &AIcon)
{
  if (FStatusItems.contains(AStatusId))
  {
    StatusItem *status = FStatusItems.value(AStatusId);
    status->icon = AIcon;
    status->iconsetFile.clear();
    status->iconName.clear();
    updateStatusActions(AStatusId);
  }
}

void StatusChanger::setStatusItemIcon(int AStatusId, const QString &AIconsetFile, const QString &AIconName)
{
  if (FStatusItems.contains(AStatusId))
  {
    StatusItem *status = FStatusItems.value(AStatusId);
    status->icon = QIcon();
    status->iconsetFile = AIconsetFile;
    status->iconName = AIconName;
    updateStatusActions(AStatusId);
  }
}

int StatusChanger::statusByName(const QString &AName) const
{
  foreach(StatusItem *status, FStatusItems)
    if (status->name.toLower() == AName.toLower())
      return status->code;
  return NULL_STATUS_ID;
}

QList<int> StatusChanger::statusByShow(int AShow) const
{
  QList<int> statuses;
  foreach(StatusItem *status, FStatusItems)
    if (status->show == AShow)
      statuses.append(status->code);
  return statuses;
}

QIcon StatusChanger::iconByShow(int AShow) const
{
  return FStatusIcons != NULL ? FStatusIcons->iconByStatus(AShow,"",false) : QIcon();
}

QString StatusChanger::nameByShow(int AShow) const 
{
  switch (AShow)
  {
  case IPresence::Offline: 
    return tr("Offline");
  case IPresence::Online: 
    return tr("Online");
  case IPresence::Chat: 
    return tr("Chat");
  case IPresence::Away: 
    return tr("Away");
  case IPresence::ExtendedAway: 
    return tr("Extended Away");
  case IPresence::DoNotDistrib: 
    return tr("Do not disturb");
  case IPresence::Invisible: 
    return tr("Invisible");
  case IPresence::Error: 
    return tr("Error");
  default:
    return tr("Unknown Status");
  }
}

void StatusChanger::createDefaultStatus()
{
  StatusItem *status = new StatusItem;
  status->code = STATUS_ONLINE;
  status->name = nameByShow(IPresence::Online);
  status->show = IPresence::Online;
  status->text = tr("Online");
  status->priority = 30;
  FStatusItems.insert(status->code,status);
  createStatusActions(status->code);

  status = new StatusItem;
  status->code = STATUS_CHAT;
  status->name = nameByShow(IPresence::Chat);
  status->show = IPresence::Chat;
  status->text = tr("Free for chat");
  status->priority = 25;
  FStatusItems.insert(status->code,status);
  createStatusActions(status->code);

  status = new StatusItem;
  status->code = STATUS_AWAY;
  status->name = nameByShow(IPresence::Away);
  status->show = IPresence::Away;
  status->text = tr("I`am away from my desk");
  status->priority = 20;
  FStatusItems.insert(status->code,status);
  createStatusActions(status->code);

  status = new StatusItem;
  status->code = STATUS_DND;
  status->name = nameByShow(IPresence::DoNotDistrib);
  status->show = IPresence::DoNotDistrib;
  status->text = tr("Do not disturb");
  status->priority = 15;
  FStatusItems.insert(status->code,status);
  createStatusActions(status->code);

  status = new StatusItem;
  status->code = STATUS_EXAWAY;
  status->name = nameByShow(IPresence::ExtendedAway);
  status->show = IPresence::ExtendedAway;
  status->text = tr("Not available");
  status->priority = 10;
  FStatusItems.insert(status->code,status);
  createStatusActions(status->code);

  status = new StatusItem;
  status->code = STATUS_INVISIBLE;
  status->name = nameByShow(IPresence::Invisible);
  status->show = IPresence::Invisible;
  status->text = tr("Disconnected");
  status->priority = 5;
  FStatusItems.insert(status->code,status);
  createStatusActions(status->code);

  status = new StatusItem;
  status->code = STATUS_OFFLINE;
  status->name = nameByShow(IPresence::Offline);
  status->show = IPresence::Offline;
  status->text = tr("Disconnected");
  status->priority = 0;
  FStatusItems.insert(status->code,status);
  createStatusActions(status->code);

  status = new StatusItem;
  status->code = STATUS_ERROR;
  status->name = nameByShow(IPresence::Error);
  status->show = IPresence::Error;
  status->text = tr("Error");
  status->priority = 0;
  FStatusItems.insert(status->code,status);
}

void StatusChanger::setMainStatusId(int AStatusId)
{
  if (FStatusItems.contains(AStatusId))
  {
    FStatusItems[MAIN_STATUS_ID] = FStatusItems.value(AStatusId);
    updateMainStatusActions();
  }
}

void StatusChanger::setStreamStatusId(IPresence *APresence, int AStatusId)
{
  if (FStatusItems.contains(AStatusId))
  {
    FStreamStatus[APresence] = AStatusId;
    if (AStatusId > MAX_TEMP_STATUS_ID)
      removeTempStatus(APresence);

    IRosterIndex *index = FRostersView && FRostersModel ? FRostersModel->streamRoot(APresence->streamJid()) : NULL;
    if (APresence->show() == IPresence::Error)
    {
      if (index && !FRostersViewPlugin->checkOption(IRostersView::ShowStatusText))
        FRostersView->insertFooterText(FTO_STATUS,APresence->status(),index);
      if (!FStreamNotify.contains(APresence))
        insertStatusNotification(APresence);
    }
    else
    {
      if (index && !FRostersViewPlugin->checkOption(IRostersView::ShowStatusText))
        FRostersView->removeFooterText(FTO_STATUS,index);
      removeStatusNotification(APresence);
    }
  }
}

Action *StatusChanger::createStatusAction(int AStatusId, const Jid &AStreamJid, QObject *AParent) const
{
  StatusItem *status = FStatusItems.value(AStatusId);

  Action *action = new Action(AParent);
  if (AStreamJid.isValid())
    action->setData(Action::DR_StreamJid,AStreamJid.full());
  action->setData(ACTION_DR_STATUS_CODE,AStatusId);
  connect(action,SIGNAL(triggered(bool)),SLOT(onSetStatusByAction(bool)));
  updateStatusAction(status,action);

  return action;
}

void StatusChanger::updateStatusAction(StatusItem *AStatus, Action *AAction) const
{
  AAction->setText(AStatus->name);
  if (!AStatus->iconsetFile.isEmpty() && !AStatus->iconName.isEmpty())
  {
    AAction->setIcon(AStatus->iconsetFile,AStatus->iconName);
    AStatus->icon = AAction->icon();
  }
  else if (!AStatus->icon.isNull())
    AAction->setIcon(AStatus->icon);
  else
    AAction->setIcon(iconByShow(AStatus->show));

  int sortShow = AStatus->show != IPresence::Offline ? AStatus->show : 100;
  AAction->setData(Action::DR_SortString,QString("%1-%2").arg(sortShow,5,10,QChar('0')).arg(AStatus->name));
}

void StatusChanger::createStatusActions(int AStatusId)
{
  FMainMenu->addAction(createStatusAction(AStatusId,Jid(),FMainMenu),AG_DEFAULT,true);
}

void StatusChanger::updateStatusActions(int AStatusId)
{
  StatusItem *status = FStatusItems.value(AStatusId);

  QMultiHash<int, QVariant> data;
  data.insert(ACTION_DR_STATUS_CODE,AStatusId);
  QList<Action *> actionList = FMainMenu->findActions(data,true);
  foreach (Action *action, actionList)
    updateStatusAction(status,action);
}

void StatusChanger::removeStatusActions(int AStatusId)
{
  QMultiHash<int, QVariant> data;
  data.insert(ACTION_DR_STATUS_CODE,AStatusId);
  qDeleteAll(FMainMenu->findActions(data,true));
}

void StatusChanger::createStreamMenu(IPresence *APresence)
{
  if (!FStreamMenu.contains(APresence))
  {
    Jid streamJid = APresence->streamJid();
    IAccount *account = FAccountManager!=NULL ? FAccountManager->accountByStream(streamJid) : NULL;

    Menu *sMenu = new Menu(FMainMenu);
    if (account)
    {
      sMenu->setTitle(account->name());
      connect(account->instance(),SIGNAL(changed(const QString &, const QVariant &)),SLOT(onAccountChanged(const QString &, const QVariant &)));
    }
    else
      sMenu->setTitle(APresence->streamJid().hFull());
    FStreamMenu.insert(APresence,sMenu);

    QHash<int, StatusItem *>::const_iterator it = FStatusItems.constBegin();
    while (it != FStatusItems.constEnd())
    {
      if (it.key() > NULL_STATUS_ID)
        sMenu->addAction(createStatusAction(it.key(),streamJid,sMenu),AG_DEFAULT,true);
      it++;
    }

    Action *action = createStatusAction(MAIN_STATUS_ID, streamJid, sMenu);
    action->setData(ACTION_DR_STATUS_CODE, MAIN_STATUS_ID);
    sMenu->addAction(action,AG_STATUSCHANGER_STATUSMENU_STREAMS,true);
    FStreamMainStatusAction.insert(APresence,action);

    FMainMenu->addAction(sMenu->menuAction(),AG_STATUSCHANGER_STATUSMENU_STREAMS,true);
  }
}

void StatusChanger::updateStreamMenu(IPresence *APresence)
{
  int statusId = FStreamStatus.value(APresence,MAIN_STATUS_ID);

  Menu *sMenu = FStreamMenu.value(APresence);
  if (sMenu)
  {
    QIcon icon = statusItemIcon(statusId);
    if (icon.isNull())
      sMenu->setIcon(iconByShow(statusItemShow(statusId)));
    else
      sMenu->setIcon(icon);
  }

  Action *mAction = FStreamMainStatusAction.value(APresence);
  if (mAction)
    mAction->setVisible(FStreamStatus.value(APresence) != MAIN_STATUS_ID);
}

void StatusChanger::removeStreamMenu(IPresence *APresence)
{
  if (FStreamMenu.contains(APresence))
  {
    FStreamMainStatusAction.remove(APresence);
    FStreamStatus.remove(APresence);
    FStreamWaitStatus.remove(APresence);
    FStreamLastStatus.remove(APresence->streamJid());
    FStreamWaitReconnect.remove(APresence);
    removeTempStatus(APresence);
    delete FStreamMenu.take(APresence);
  }
}

void StatusChanger::updateMainMenu()
{
  int mainStatusToShow = MAIN_STATUS_ID;
  
  if (FStreamStatus.isEmpty())
  {
    mainStatusToShow = STATUS_OFFLINE;
    FMainMenu->menuAction()->setEnabled(false);
  }
  else
  {
    QList<int> statusOnline, statusOffline;
    QHash<IPresence *, int>::const_iterator it = FStreamStatus.constBegin();
    while (it != FStreamStatus.constEnd())
    {
      if (it.key()->xmppStream()->isOpen())
        statusOnline.append(it.value());
      else
        statusOffline.append(it.value());
      it++;
    }
    if (!statusOnline.isEmpty() && !statusOnline.contains(MAIN_STATUS_ID))
      mainStatusToShow = statusOnline.first();
    else if (statusOnline.isEmpty() && !statusOffline.contains(MAIN_STATUS_ID))
      mainStatusToShow = statusOffline.first();

    FMainMenu->menuAction()->setEnabled(true);
  }
  
  QIcon mStatusIcon = statusItemIcon(mainStatusToShow);
  if (mStatusIcon.isNull())
    mStatusIcon = iconByShow(statusItemShow(mainStatusToShow));
  QString mStatusName = statusItemName(mainStatusToShow);
  FMainMenu->setIcon(mStatusIcon);
  FMainMenu->setTitle(mStatusName);

  if (FTrayManager)
    FTrayManager->setMainIcon(mStatusIcon);
}

void StatusChanger::updateTrayToolTip()
{
  QString trayToolTip;
  QHash<IPresence *, int>::const_iterator it = FStreamStatus.constBegin();
  while (it != FStreamStatus.constEnd())
  {
    IAccount *account = FAccountManager->accountByStream(it.key()->streamJid());
    if (!trayToolTip.isEmpty())
      trayToolTip+="\n";
    trayToolTip += tr("%1 - %2").arg(account->name()).arg(it.key()->status());
    it++;
  }
  if (FTrayManager)
    FTrayManager->setMainToolTip(trayToolTip);
}

void StatusChanger::updateMainStatusActions()
{
  QIcon mStatusIcon = statusItemIcon(MAIN_STATUS_ID);
  if (mStatusIcon.isNull())
    mStatusIcon = iconByShow(statusItemShow(MAIN_STATUS_ID));
  QString mStatusName = statusItemName(MAIN_STATUS_ID);
  foreach (Action *action, FStreamMainStatusAction)
  {
    action->setIcon(mStatusIcon);
    action->setText(mStatusName);
  }
}

void StatusChanger::insertConnectingLabel(IPresence *APresence)
{
  if (FRostersModel && FRostersView)
  {
    IRosterIndex *index = FRostersModel->streamRoot(APresence->xmppStream()->jid());
    if (index)
      FRostersView->insertIndexLabel(FConnectingLabel,index);
  }
}

void StatusChanger::removeConnectingLabel(IPresence *APresence)
{
  if (FRostersModel && FRostersView)
  {
    IRosterIndex *index = FRostersModel->streamRoot(APresence->xmppStream()->jid());
    if (index)
      FRostersView->removeIndexLabel(FConnectingLabel,index);
  }
}

void StatusChanger::autoReconnect(IPresence *APresence)
{
  IAccount *account = FAccountManager!=NULL ? FAccountManager->accountByStream(APresence->streamJid()) : NULL;
  if (account && account->value(AVN_AUTORECONNECT,true).toBool())
  {
    int statusId = FStreamWaitStatus.value(APresence, FStreamStatus.value(APresence));
    int statusShow = statusItemShow(statusId);
    if (statusShow != IPresence::Offline && statusShow != IPresence::Error)
    {
      int waitTime = account->value(AVN_RECONNECT_TIME,30).toInt();
      FStreamWaitReconnect.insert(APresence,QPair<QDateTime,int>(QDateTime::currentDateTime().addSecs(waitTime),statusId));
      QTimer::singleShot(waitTime*1000+100,this,SLOT(onReconnectTimer()));
    }
  }
}

int StatusChanger::createTempStatus(IPresence *APresence, int AShow, const QString &AText, int APriority)
{
  removeTempStatus(APresence);
  StatusItem *status = new StatusItem;
  status->name = nameByShow(AShow).append('*');
  status->show = AShow;
  status->text = AText;
  status->priority = APriority;
  status->icon = iconByShow(AShow);
  status->code = MAX_TEMP_STATUS_ID;
  while (FStatusItems.contains(status->code))
    status->code--;
  FStatusItems.insert(status->code,status);
  FStreamTempStatus.insert(APresence,status->code);
  return status->code;
}

void StatusChanger::removeTempStatus(IPresence *APresence)
{
  if (FStreamTempStatus.contains(APresence))
    if (!activeStatusItems().contains(FStreamTempStatus.value(APresence)))
      delete FStatusItems.take(FStreamTempStatus.take(APresence));
}

void StatusChanger::resendUpdatedStatus(int AStatusId)
{
  if (FStatusItems[MAIN_STATUS_ID]->code == AStatusId)
    setStatus(AStatusId);

  QHash<IPresence *,int>::const_iterator it;
  for (it = FStreamStatus.constBegin(); it != FStreamStatus.constEnd(); it++)
    if (it.value() == AStatusId)
      setStatus(AStatusId, it.key()->streamJid());
}

void StatusChanger::removeAllCustomStatuses()
{
  QList<int> statusList = FStatusItems.keys();
  foreach (int statusId, statusList)
    if (statusId > MAX_STANDART_STATUS_ID)
      removeStatusItem(statusId);
}

void StatusChanger::insertStatusNotification(IPresence *APresence)
{
  if (FNotifications)
  {
    removeStatusNotification(APresence);
    if (FNotifications)
    {
      INotification notify;
      notify.kinds = FNotifications->notificatorKinds(NOTIFICATOR_ID);
      notify.data.insert(NDR_ICON,FStatusIcons!=NULL ? FStatusIcons->iconByStatus(IPresence::Error,"","") : QIcon());
      notify.data.insert(NDR_WINDOW_CAPTION, tr("Connection error"));
      notify.data.insert(NDR_WINDOW_TITLE,FAccountManager!=NULL ? FAccountManager->accountByStream(APresence->streamJid())->name() : APresence->streamJid().full());
      notify.data.insert(NDR_WINDOW_IMAGE, FNotifications->contactAvatar(APresence->streamJid()));
      notify.data.insert(NDR_WINDOW_TEXT,APresence->status());
      notify.data.insert(NDR_WINDOW_TEXT,APresence->status());
      notify.data.insert(NDR_SOUND_FILE,SDF_SCHANGER_CONNECTION_ERROR);
      FStreamNotify.insert(APresence,FNotifications->appendNotification(notify));
    }
  }
}

void StatusChanger::removeStatusNotification(IPresence *APresence)
{
  if (FNotifications && FStreamNotify.contains(APresence))
  {
    FNotifications->removeNotification(FStreamNotify.take(APresence));
  }
}

void StatusChanger::onSetStatusByAction(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(Action::DR_StreamJid).toString();
    int statusId = action->data(ACTION_DR_STATUS_CODE).toInt();
    setStatus(statusId,streamJid);
  }
}

void StatusChanger::onPresenceAdded(IPresence *APresence)
{
  if (FStreamMenu.count() == 1)
    FStreamMenu.value(FStreamMenu.keys().first())->menuAction()->setVisible(true);

  createStreamMenu(APresence);
  FStreamStatus.insert(APresence,STATUS_OFFLINE);

  if (FStreamMenu.count() == 1)
    FStreamMenu.value(FStreamMenu.keys().first())->menuAction()->setVisible(false);

  IAccount *account = FAccountManager!=NULL ? FAccountManager->accountByStream(APresence->streamJid()) : NULL;
  if (account && account->value(AVN_IS_MAIN_STATUS, true).toBool())
    FStreamMainStatus += APresence;
  
  updateStreamMenu(APresence);
  updateMainMenu();
  updateTrayToolTip();
}

void StatusChanger::onPresenceChanged(IPresence *APresence, int AShow, const QString &AText, int APriority)
{
  if (FStreamStatus.contains(APresence))
  {
    if (AShow == IPresence::Error)
    {
      autoReconnect(APresence);
      setStreamStatusId(APresence, STATUS_ERROR);
      updateStreamMenu(APresence);
      updateMainMenu();
    }
    else if (FSettingStatusToPresence != APresence)
    {
      StatusItem *item = FStatusItems.value(FStreamStatus.value(APresence),NULL);
      if (!item || item->show!=AShow || item->priority!=APriority || item->text!=AText)
      {
        setStreamStatusId(APresence, createTempStatus(APresence,AShow,AText,APriority));
        updateStreamMenu(APresence);
        updateMainMenu();
      }
    }
    if (FStreamWaitStatus.contains(APresence))
    {
      FStreamWaitStatus.remove(APresence);
      removeConnectingLabel(APresence);
    }
    updateTrayToolTip();
  }
}

void StatusChanger::onPresenceRemoved(IPresence *APresence)
{
  IAccount *account = FAccountManager!=NULL ? FAccountManager->accountByStream(APresence->streamJid()) : NULL;
  if (account)
  {
    bool isMainStatus = FStreamMainStatus.contains(APresence);
    account->setValue(AVN_IS_MAIN_STATUS,isMainStatus);
    if (!isMainStatus && account->value(AVN_AUTOCONNECT,false).toBool() && FStreamLastStatus.contains(APresence->streamJid()))  
      account->setValue(AVN_LAST_ONLINE_STATUS,FStreamLastStatus.take(APresence->streamJid()));
    else
      account->delValue(AVN_LAST_ONLINE_STATUS);
  }

  removeStatusNotification(APresence);
  removeStreamMenu(APresence);

  if (FStreamMenu.count() == 1)
    FStreamMenu.value(FStreamMenu.keys().first())->menuAction()->setVisible(false);
  
  updateMainMenu();
  updateTrayToolTip();
}

void StatusChanger::onRosterOpened(IRoster *ARoster)
{
  IPresence *presence = FPresencePlugin->getPresence(ARoster->streamJid());
  if (FStreamWaitStatus.contains(presence))
    setStatus(FStreamWaitStatus.value(presence),presence->streamJid());
}

void StatusChanger::onRosterClosed(IRoster *ARoster)
{
  IPresence *presence = FPresencePlugin->getPresence(ARoster->streamJid());
  if (FStreamWaitStatus.contains(presence))
    setStatus(FStreamWaitStatus.value(presence),presence->streamJid());
}

void StatusChanger::onStreamJidChanged(const Jid &ABefour, const Jid &AAfter)
{
  QMultiHash<int,QVariant> data;
  data.insert(Action::DR_StreamJid,ABefour.full());
  QList<Action *> actionList = FMainMenu->findActions(data,true);
  foreach (Action *action, actionList)
    action->setData(Action::DR_StreamJid,AAfter.full());
}

void StatusChanger::onRostersViewContextMenu(IRosterIndex *AIndex, Menu *AMenu)
{
  if (AIndex->data(RDR_Type).toInt() == RIT_StreamRoot)
  {
    QString streamJid = AIndex->data(RDR_StreamJid).toString();
    Menu *menu = FStreamMenu.count() > 1 ? streamMenu(streamJid) : FMainMenu;
    if (menu)
    {
      Action *action = new Action(AMenu);
      action->setText(tr("Status"));
      action->setMenu(menu);
      action->setIcon(menu->menuAction()->icon());
      AMenu->addAction(action,AG_STATUSCHANGER_ROSTER,true);
    }
  }
}

void StatusChanger::onDefaultStatusIconsChanged()
{
  foreach (StatusItem *statusItem, FStatusItems)
    updateStatusActions(statusItem->code);
  foreach (IPresence *presence, FStreamMenu.keys())
    updateStreamMenu(presence);
  updateMainStatusActions();
  updateMainMenu();
}

void StatusChanger::onSettingsOpened()
{
  removeAllCustomStatuses();

  ISettings *settings = FSettingsPlugin->settingsForPlugin(STATUSCHANGER_UUID);
  QList<QString> nsList = settings->values(SVN_STATUS_CODE).keys();
  foreach (QString ns, nsList)
  {
    int statusId = settings->valueNS(SVN_STATUS_CODE,ns).toInt();
    QString statusName = settings->valueNS(SVN_STATUS_NAME,ns).toString();
    if (statusId > MAX_STANDART_STATUS_ID)
    {
      if (!statusName.isEmpty() && statusByName(statusName)==NULL_STATUS_ID)
      {
        StatusItem *status = new StatusItem;
        status->code = statusId;
        status->name = statusName;
        status->show = (IPresence::Show)settings->valueNS(SVN_STATUS_SHOW,ns).toInt();
        status->text = settings->valueNS(SVN_STATUS_TEXT,ns).toString();
        status->priority = settings->valueNS(SVN_STATUS_PRIORITY,ns).toInt();
        status->iconsetFile = settings->valueNS(SVN_STATUS_ICONSET,ns).toString();
        status->iconName = settings->valueNS(SVN_STATUS_ICON_NAME,ns).toString();
        FStatusItems.insert(status->code,status);
        createStatusActions(status->code);
      }
    }
    else if (statusId > NULL_STATUS_ID)
    {
      StatusItem *status = FStatusItems.value(statusId);
      if (status)
      {
        if (!statusName.isEmpty())
          status->name = statusName;
        status->text = settings->valueNS(SVN_STATUS_TEXT,ns,status->text).toString();
        status->priority = settings->valueNS(SVN_STATUS_PRIORITY,ns,status->priority).toInt();
        updateStatusActions(statusId);
      }
    }
  }

  int mainStatusId = settings->value(SVN_MAIN_STATUS_ID,STATUS_OFFLINE).toInt();
  setMainStatusId(mainStatusId);
}

void StatusChanger::onSettingsClosed()
{
  if (!FEditStatusDialog.isNull())
    FEditStatusDialog->reject();

  ISettings *settings = FSettingsPlugin->settingsForPlugin(STATUSCHANGER_UUID);
  QSet<QString> oldNS = settings->values(SVN_STATUS_CODE).keys().toSet();
  foreach (StatusItem *status, FStatusItems)
  {
    QString ns = QString::number(status->code);
    if (status->code > MAX_STANDART_STATUS_ID)
    {
      settings->setValueNS(SVN_STATUS_CODE,ns,status->code);
      settings->setValueNS(SVN_STATUS_NAME,ns,status->name);
      settings->setValueNS(SVN_STATUS_SHOW,ns,status->show);
      settings->setValueNS(SVN_STATUS_TEXT,ns,status->text);
      settings->setValueNS(SVN_STATUS_PRIORITY,ns,status->priority);
      settings->setValueNS(SVN_STATUS_ICONSET,ns,status->iconsetFile);
      settings->setValueNS(SVN_STATUS_ICON_NAME,ns,status->iconName);
    }
    else if (status->code > NULL_STATUS_ID)
    {
      settings->setValueNS(SVN_STATUS_CODE,ns,status->code);
      settings->setValueNS(SVN_STATUS_NAME,ns,status->name);
      settings->setValueNS(SVN_STATUS_TEXT,ns,status->text);
      settings->setValueNS(SVN_STATUS_PRIORITY,ns,status->priority);
    }
    oldNS -= ns;
  }

  foreach(QString ns, oldNS)
    settings->deleteValueNS(SVN_STATUS,ns);

  settings->setValue(SVN_MAIN_STATUS_ID,FStatusItems.value(MAIN_STATUS_ID)->code);
  setMainStatusId(STATUS_OFFLINE);

  removeAllCustomStatuses();
}

void StatusChanger::onReconnectTimer()
{
  QHash<IPresence *,QPair<QDateTime,int> >::iterator it = FStreamWaitReconnect.begin();
  while (it != FStreamWaitReconnect.end())
  {
    if (it.value().first <= QDateTime::currentDateTime()) 
    {
      IPresence *presence = it.key();
      int statusId = FStatusItems.contains(it.value().second) ? it.value().second : STATUS_ONLINE;
      it = FStreamWaitReconnect.erase(it);
      if (presence->show() == IPresence::Error)
        setStatus(statusId,presence->streamJid());
    }
    else
    {
      it++;
    }
  }
}

void StatusChanger::onEditStatusAction(bool)
{
  if (FEditStatusDialog.isNull())
  {
    FEditStatusDialog = new EditStatusDialog(this);
    FEditStatusDialog->show();
  }
  else
    FEditStatusDialog->show();
}

void StatusChanger::onOptionsAccepted()
{
  foreach (AccountOptionsWidget *widget, FAccountOptionsById)
  {
    IAccount *account = FAccountManager->accountById(widget->accountId());
    if (account)
    {
      account->setValue(AVN_AUTOCONNECT,widget->autoConnect());
      account->setValue(AVN_AUTORECONNECT, widget->autoReconnect());
    }
  }
  emit optionsAccepted();
}

void StatusChanger::onOptionsRejected()
{
  emit optionsRejected();
}

void StatusChanger::onOptionsDialogClosed()
{
  FAccountOptionsById.clear();
}

void StatusChanger::onAccountChanged(const QString &AName, const QVariant &AValue)
{
  if (AName == AVN_NAME)
  {
    IAccount *account = qobject_cast<IAccount *>(sender());
    if (account)
    {
      Menu *sMenu = streamMenu(account->streamJid());
      if (sMenu)
        sMenu->setTitle(AValue.toString());
    }
  }
}

void StatusChanger::onNotificationActivated(int ANotifyId)
{
  FNotifications->removeNotification(ANotifyId);
}

Q_EXPORT_PLUGIN2(StatusChangerPlugin, StatusChanger)