#include "trayicon.h"

#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QTimer>

#include <conf/addressgroup.h>
#include <conf/appgroup.h>
#include <conf/confmanager.h>
#include <conf/firewallconf.h>
#include <driver/drivermanager.h>
#include <form/controls/clickablemenu.h>
#include <form/controls/controlutil.h>
#include <form/controls/mainwindow.h>
#include <fortsettings.h>
#include <manager/hotkeymanager.h>
#include <manager/windowmanager.h>
#include <user/iniuser.h>
#include <util/guiutil.h>
#include <util/iconcache.h>

#include "traycontroller.h"

namespace {

const QString eventSingleClick = QStringLiteral("singleClick");
const QString eventCtrlSingleClick = QStringLiteral("ctrlSingleClick");
const QString eventAltSingleClick = QStringLiteral("altSingleClick");
const QString eventDoubleClick = QStringLiteral("doubleClick");
const QString eventMiddleClick = QStringLiteral("middleClick");
const QString eventRightClick = QStringLiteral("rightClick");

const QString actionShowHome = QStringLiteral("Home");
const QString actionShowPrograms = QStringLiteral("Programs");
const QString actionShowOptions = QStringLiteral("Options");
const QString actionShowStatistics = QStringLiteral("Statistics");
const QString actionShowTrafficGraph = QStringLiteral("TrafficGraph");
const QString actionSwitchFilterEnabled = QStringLiteral("FilterEnabled");
const QString actionSwitchStopTraffic = QStringLiteral("StopTraffic");
const QString actionSwitchStopInetTraffic = QStringLiteral("StopInetTraffic");
const QString actionShowFilterModeMenu = QStringLiteral("FilterModeMenu");
const QString actionShowTrayMenu = QStringLiteral("TrayMenu");
const QString actionIgnore = QStringLiteral("Ignore");

QString clickNameByType(TrayIcon::ClickType clickType)
{
    switch (clickType) {
    case TrayIcon::SingleClick:
        return eventSingleClick;
    case TrayIcon::CtrlSingleClick:
        return eventCtrlSingleClick;
    case TrayIcon::AltSingleClick:
        return eventAltSingleClick;
    case TrayIcon::DoubleClick:
        return eventDoubleClick;
    case TrayIcon::MiddleClick:
        return eventMiddleClick;
    case TrayIcon::RightClick:
        return eventRightClick;
    default:
        return QString();
    }
}

QString actionNameByType(TrayIcon::ActionType actionType)
{
    static const QString actionNames[] = {
        actionShowHome,
        actionShowPrograms,
        actionShowOptions,
        actionShowStatistics,
        actionShowTrafficGraph,
        actionSwitchFilterEnabled,
        actionSwitchStopTraffic,
        actionSwitchStopInetTraffic,
        actionShowFilterModeMenu,
        actionShowTrayMenu,
        actionIgnore,
    };

    if (actionType > TrayIcon::ActionNone && actionType < TrayIcon::ActionTypeCount) {
        return actionNames[actionType];
    }

    return {};
}

TrayIcon::ActionType actionTypeByName(const QString &name)
{
    static const QHash<QString, TrayIcon::ActionType> actionTypeNamesMap = {
        { actionShowHome, TrayIcon::ActionShowHome },
        { actionShowPrograms, TrayIcon::ActionShowPrograms },
        { actionShowOptions, TrayIcon::ActionShowOptions },
        { actionShowStatistics, TrayIcon::ActionShowStatistics },
        { actionShowTrafficGraph, TrayIcon::ActionShowTrafficGraph },
        { actionSwitchFilterEnabled, TrayIcon::ActionSwitchFilterEnabled },
        { actionSwitchStopTraffic, TrayIcon::ActionSwitchStopTraffic },
        { actionSwitchStopInetTraffic, TrayIcon::ActionSwitchStopInetTraffic },
        { actionShowFilterModeMenu, TrayIcon::ActionShowFilterModeMenu },
        { actionShowTrayMenu, TrayIcon::ActionShowTrayMenu },
        { actionIgnore, TrayIcon::ActionIgnore }
    };

    return name.isEmpty() ? TrayIcon::ActionNone
                          : actionTypeNamesMap.value(name, TrayIcon::ActionNone);
}

TrayIcon::ActionType defaultActionTypeByClick(TrayIcon::ClickType clickType)
{
    switch (clickType) {
    case TrayIcon::SingleClick:
        return TrayIcon::ActionShowPrograms;
    case TrayIcon::CtrlSingleClick:
        return TrayIcon::ActionShowOptions;
    case TrayIcon::AltSingleClick:
        return TrayIcon::ActionIgnore;
    case TrayIcon::DoubleClick:
        return TrayIcon::ActionIgnore;
    case TrayIcon::MiddleClick:
        return TrayIcon::ActionShowStatistics;
    case TrayIcon::RightClick:
        return TrayIcon::ActionShowTrayMenu;
    default:
        return TrayIcon::ActionNone;
    }
}

void setActionCheckable(QAction *action, bool checked = false, const QObject *receiver = nullptr,
        const char *member = nullptr)
{
    action->setCheckable(true);
    action->setChecked(checked);

    if (receiver) {
        QObject::connect(action, SIGNAL(toggled(bool)), receiver, member);
    }
}

QAction *addAction(QWidget *widget, const QIcon &icon, const QString &text,
        const QObject *receiver = nullptr, const char *member = nullptr, bool checkable = false,
        bool checked = false)
{
    auto action = new QAction(icon, text, widget);

    if (receiver) {
        QObject::connect(action, SIGNAL(triggered(bool)), receiver, member);
    }
    if (checkable) {
        setActionCheckable(action, checked);
    }

    widget->addAction(action);

    return action;
}

}

TrayIcon::TrayIcon(QObject *parent) :
    QSystemTrayIcon(parent),
    m_trayTriggered(false),
    m_alerted(false),
    m_animatedAlert(false),
    m_ctrl(new TrayController(this))
{
    setupUi();
    setupController();

    connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::onTrayActivated);

    connect(confManager(), &ConfManager::confChanged, this, &TrayIcon::updateTrayMenu);
    connect(confManager(), &ConfManager::iniUserChanged, this, &TrayIcon::updateTrayMenu);
    connect(driverManager(), &DriverManager::isDeviceOpenedChanged, this,
            &TrayIcon::updateTrayIconShape);
}

FortSettings *TrayIcon::settings() const
{
    return ctrl()->settings();
}

ConfManager *TrayIcon::confManager() const
{
    return ctrl()->confManager();
}

FirewallConf *TrayIcon::conf() const
{
    return ctrl()->conf();
}

IniOptions *TrayIcon::ini() const
{
    return ctrl()->ini();
}

IniUser *TrayIcon::iniUser() const
{
    return ctrl()->iniUser();
}

HotKeyManager *TrayIcon::hotKeyManager() const
{
    return ctrl()->hotKeyManager();
}

DriverManager *TrayIcon::driverManager() const
{
    return ctrl()->driverManager();
}

WindowManager *TrayIcon::windowManager() const
{
    return ctrl()->windowManager();
}

void TrayIcon::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (WindowManager::activateModalWidget())
        return;

    switch (reason) {
    case QSystemTrayIcon::Trigger: {
        onTrayActivatedByTrigger();
    } break;
    case QSystemTrayIcon::DoubleClick: {
        onTrayActivatedByClick(DoubleClick, /*checkTriggered=*/true);
    } break;
    case QSystemTrayIcon::MiddleClick: {
        onTrayActivatedByClick(MiddleClick);
    } break;
    case QSystemTrayIcon::Context: {
        onTrayActivatedByClick(RightClick);
    } break;
    default:
        break;
    }
}

void TrayIcon::updateTrayIcon(bool alerted)
{
    if (m_alerted == alerted)
        return;

    m_alerted = alerted;
    m_animatedAlert = false;

    updateAlertTimer();

    updateTrayIconShape();
}

void TrayIcon::showTrayMenu(const QPoint &pos)
{
    m_menu->popup(pos);
}

void TrayIcon::updateTrayMenu(bool onlyFlags)
{
    if (!onlyFlags) {
        updateAppGroupActions();
    }

    updateTrayMenuFlags();
    updateTrayIconShape();
    updateHotKeys();
    updateClickActions();
}

void TrayIcon::quitProgram()
{
    if (iniUser()->confirmQuit()) {
        windowManager()->showConfirmBox(
                [&] { windowManager()->quit(); }, tr("Are you sure you want to quit the program?"));
    } else {
        windowManager()->quit();
    }
}

void TrayIcon::switchTrayMenu(bool /*checked*/)
{
    showTrayMenu(QCursor::pos());
}

void TrayIcon::switchFilterModeMenu(bool /*checked*/)
{
    m_filterModeMenu->popup(QCursor::pos());
}

void TrayIcon::setupController()
{
    connect(windowManager(), &WindowManager::optWindowChanged, this,
            &TrayIcon::updateTrayMenuFlags);
    connect(windowManager(), &WindowManager::graphWindowChanged, m_graphAction,
            &QAction::setChecked);

    connect(settings(), &FortSettings::passwordCheckedChanged, this,
            &TrayIcon::updateTrayMenuFlags);

    connect(ctrl(), &TrayController::retranslateUi, this, &TrayIcon::retranslateUi);

    retranslateUi();
}

void TrayIcon::retranslateUi()
{
    m_homeAction->setText(tr("My Fort"));
    m_programsAction->setText(tr("Programs"));
    m_optionsMenu->setTitle(tr("Options"));
    m_optionsAction->setText(tr("Options"));
    m_policiesAction->setText(tr("Policies"));
    m_zonesAction->setText(tr("Zones"));
    m_statisticsAction->setText(tr("Statistics"));
    m_graphAction->setText(tr("Traffic Graph"));

    m_filterEnabledAction->setText(tr("Filter Enabled"));
    m_stopTrafficAction->setText(tr("Stop Traffic"));
    m_stopInetTrafficAction->setText(tr("Stop Internet Traffic"));

    m_filterModeMenu->setTitle(tr("Filter Mode"));
    retranslateFilterModeActions();

    m_quitAction->setText(tr("Quit"));
}

void TrayIcon::retranslateFilterModeActions()
{
    int index = 0;
    for (const QString &name : FirewallConf::filterModeNames()) {
        QAction *a = m_filterModeActions->actions().at(index++);
        a->setText(name);
    }
}

void TrayIcon::setupUi()
{
    this->setToolTip(QApplication::applicationDisplayName());

    setupTrayMenu();
    updateTrayMenu();
}

void TrayIcon::setupTrayMenu()
{
    m_menu = ControlUtil::createMenu(windowManager()->mainWindow());

    m_homeAction = addAction(m_menu, IconCache::icon(":/icons/fort.png"), QString(),
            windowManager(), SLOT(showHomeWindow()));
    addHotKey(m_homeAction, iniUser()->hotKeyHome());

    m_programsAction = addAction(m_menu, IconCache::icon(":/icons/application.png"), QString(),
            windowManager(), SLOT(showProgramsWindow()));
    addHotKey(m_programsAction, iniUser()->hotKeyPrograms());

    setupTrayMenuOptions();
    m_menu->addMenu(m_optionsMenu);

    m_statisticsAction = addAction(m_menu, IconCache::icon(":/icons/chart_bar.png"), QString(),
            windowManager(), SLOT(showStatisticsWindow()));
    addHotKey(m_statisticsAction, iniUser()->hotKeyStatistics());

    m_graphAction = addAction(m_menu, IconCache::icon(":/icons/action_log.png"), QString(),
            windowManager(), SLOT(switchGraphWindow()), true, !!windowManager()->graphWindow());
    addHotKey(m_graphAction, iniUser()->hotKeyGraph());

    m_menu->addSeparator();

    m_filterEnabledAction =
            addAction(m_menu, QIcon(), QString(), this, SLOT(switchTrayFlag(bool)), true);
    addHotKey(m_filterEnabledAction, iniUser()->hotKeyFilter());

    m_stopTrafficAction =
            addAction(m_menu, QIcon(), QString(), this, SLOT(switchTrayFlag(bool)), true);
    addHotKey(m_stopTrafficAction, iniUser()->hotKeyStopTraffic());

    m_stopInetTrafficAction =
            addAction(m_menu, QIcon(), QString(), this, SLOT(switchTrayFlag(bool)), true);
    addHotKey(m_stopInetTrafficAction, iniUser()->hotKeyStopInetTraffic());

    m_filterModeMenuAction =
            addAction(m_menu, QIcon(), QString(), this, SLOT(switchFilterModeMenu(bool)));
    m_filterModeMenuAction->setVisible(false);

    setupTrayMenuFilterMode();
    m_menu->addMenu(m_filterModeMenu);

    m_menu->addSeparator();

    for (int i = 0; i < MAX_APP_GROUP_COUNT; ++i) {
        QAction *a = addAction(
                m_menu, QIcon(), QString(), this, SLOT(switchTrayFlag(bool)), /*checkable=*/true);

        if (i < 12) {
            const QString shortcutText =
                    iniUser()->hotKeyAppGroupModifiers() + "+F" + QString::number(i + 1);

            addHotKey(a, shortcutText);
        }

        m_appGroupActions.append(a);
    }

    m_menu->addSeparator();

    m_quitAction = addAction(m_menu, QIcon(), tr("Quit"), this, SLOT(quitProgram()));
    addHotKey(m_quitAction, iniUser()->hotKeyQuit());

    m_trayMenuAction = addAction(m_menu, QIcon(), QString(), this, SLOT(switchTrayMenu(bool)));
    m_trayMenuAction->setVisible(false);
}

void TrayIcon::setupTrayMenuOptions()
{
    m_optionsMenu = new ClickableMenu(m_menu);
    m_optionsMenu->setIcon(IconCache::icon(":/icons/cog.png"));

    m_optionsAction = addAction(m_optionsMenu, IconCache::icon(":/icons/cog.png"), QString(),
            windowManager(), SLOT(showOptionsWindow()));
    addHotKey(m_optionsAction, iniUser()->hotKeyOptions());

    connect(m_optionsMenu, &ClickableMenu::clicked, m_optionsAction, &QAction::trigger);

    m_policiesAction = addAction(m_optionsMenu, IconCache::icon(":/icons/traffic_lights.png"),
            QString(), windowManager(), SLOT(showPoliciesWindow()));
    addHotKey(m_policiesAction, iniUser()->hotKeyPolicies());

    // TODO: Implement Policies
    m_policiesAction->setEnabled(false);

    m_zonesAction = addAction(m_optionsMenu, IconCache::icon(":/icons/ip_class.png"), QString(),
            windowManager(), SLOT(showZonesWindow()));
    addHotKey(m_zonesAction, iniUser()->hotKeyZones());
}

void TrayIcon::setupTrayMenuFilterMode()
{
    m_filterModeMenu = ControlUtil::createMenu(m_menu);

    m_filterModeActions = new QActionGroup(m_filterModeMenu);

    int index = 0;
    const QStringList iconPaths = FirewallConf::filterModeIconPaths();
    const QStringList hotKeys = IniUser::filterModeHotKeys();
    for (const QString &name : FirewallConf::filterModeNames()) {
        const QString iconPath = iconPaths.at(index);
        const QString hotKey = hotKeys.at(index);

        QAction *a = addAction(m_filterModeMenu, IconCache::icon(iconPath), name,
                /*receiver=*/nullptr,
                /*member=*/nullptr, /*checkable=*/true);
        addHotKey(a, iniUser()->hotKeyValue(hotKey));

        // TODO: Implement Ask to Connect
        a->setEnabled(index != 1);

        m_filterModeActions->addAction(a);
        ++index;
    }

    connect(m_filterModeActions, &QActionGroup::triggered, this, &TrayIcon::switchFilterMode);
}

void TrayIcon::updateTrayMenuFlags()
{
    const bool editEnabled = (!settings()->isPasswordRequired() && !windowManager()->optWindow());

    m_filterEnabledAction->setEnabled(editEnabled);
    m_filterEnabledAction->setChecked(conf()->filterEnabled());

    m_stopTrafficAction->setEnabled(editEnabled);
    m_stopTrafficAction->setChecked(conf()->stopTraffic());

    m_stopInetTrafficAction->setEnabled(editEnabled);
    m_stopInetTrafficAction->setChecked(conf()->stopInetTraffic());

    m_filterModeMenu->setEnabled(editEnabled);
    {
        QAction *action = m_filterModeActions->actions().at(conf()->filterModeIndex());
        if (!action->isChecked()) {
            action->setChecked(true);
            m_filterModeMenu->setIcon(action->icon());
        }
    }

    int appGroupIndex = 0;
    for (QAction *action : qAsConst(m_appGroupActions)) {
        if (!action->isVisible())
            break;

        const bool appGroupEnabled = conf()->appGroupEnabled(appGroupIndex++);

        action->setEnabled(editEnabled);
        action->setChecked(appGroupEnabled);
    }
}

void TrayIcon::updateAppGroupActions()
{
    const int appGroupsCount = conf()->appGroups().count();

    for (int i = 0; i < MAX_APP_GROUP_COUNT; ++i) {
        QAction *action = m_appGroupActions.at(i);
        QString menuLabel;
        bool visible = false;

        if (i < appGroupsCount) {
            const AppGroup *appGroup = conf()->appGroups().at(i);
            menuLabel = appGroup->menuLabel();
            visible = true;
        }

        action->setText(menuLabel);
        action->setVisible(visible);
        action->setEnabled(visible);
    }
}

void TrayIcon::updateAlertTimer()
{
    if (!iniUser()->trayAnimateAlert())
        return;

    if (!m_alertTimer) {
        m_alertTimer = new QTimer(this);
        m_alertTimer->setInterval(1000);

        connect(m_alertTimer, &QTimer::timeout, this, [&] {
            m_animatedAlert = !m_animatedAlert;
            updateTrayIconShape();
        });
    }

    m_animatedAlert = m_alerted;

    if (m_alerted) {
        m_alertTimer->start();
    } else {
        m_alertTimer->stop();
    }
}

void TrayIcon::updateTrayIconShape()
{
    QString mainIconPath;

    if (!conf()->filterEnabled() || !driverManager()->isDeviceOpened()) {
        mainIconPath = ":/icons/fort_gray.png";
    } else if (conf()->stopTraffic() || conf()->stopInetTraffic()) {
        mainIconPath = ":/icons/fort_red.png";
    } else {
        mainIconPath = ":/icons/fort.png";
    }

    const auto icon = m_alerted
            ? (m_animatedAlert ? IconCache::icon(":/icons/error.png")
                               : GuiUtil::overlayIcon(mainIconPath, ":/icons/error.png"))
            : IconCache::icon(mainIconPath);

    this->setIcon(icon);
}

void TrayIcon::saveTrayFlags()
{
    conf()->setFilterEnabled(m_filterEnabledAction->isChecked());
    conf()->setStopTraffic(m_stopTrafficAction->isChecked());
    conf()->setStopInetTraffic(m_stopInetTrafficAction->isChecked());

    // Set Filter Mode
    {
        QAction *action = m_filterModeActions->checkedAction();
        const int index = m_filterModeActions->actions().indexOf(action);
        if (conf()->filterModeIndex() != index) {
            conf()->setFilterModeIndex(index);
            m_filterModeMenu->setIcon(action->icon());
        }
    }

    // Set App. Groups' enabled states
    int i = 0;
    for (AppGroup *appGroup : conf()->appGroups()) {
        const QAction *action = m_appGroupActions.at(i++);
        appGroup->setEnabled(action->isChecked());
    }

    confManager()->saveFlags();
}

void TrayIcon::switchTrayFlag(bool checked)
{
    if (iniUser()->confirmTrayFlags()) {
        const auto action = qobject_cast<QAction *>(sender());
        Q_ASSERT(action);

        windowManager()->showQuestionBox(
                [=](bool confirmed) {
                    if (confirmed) {
                        saveTrayFlags();
                    } else {
                        action->setChecked(!checked);
                    }
                },
                tr("Are you sure to switch the \"%1\"?").arg(action->text()));
    } else {
        saveTrayFlags();
    }
}

void TrayIcon::switchFilterMode(QAction *action)
{
    const int index = m_filterModeActions->actions().indexOf(action);
    if (index < 0 || index == conf()->filterModeIndex())
        return;

    if (iniUser()->confirmTrayFlags()) {
        windowManager()->showQuestionBox(
                [=](bool confirmed) {
                    if (confirmed) {
                        saveTrayFlags();
                    } else {
                        QAction *a = m_filterModeActions->actions().at(conf()->filterModeIndex());
                        a->setChecked(true);
                    }
                },
                tr("Are you sure to select the \"%1\"?").arg(action->text()));
    } else {
        saveTrayFlags();
    }
}

void TrayIcon::addHotKey(QAction *action, const QString &shortcutText)
{
    if (shortcutText.isEmpty())
        return;

    const QKeySequence shortcut = QKeySequence::fromString(shortcutText);
    hotKeyManager()->addAction(action, shortcut);
}

void TrayIcon::updateHotKeys()
{
    hotKeyManager()->initialize(iniUser()->hotKeyEnabled(), iniUser()->hotKeyGlobal());
}

void TrayIcon::removeHotKeys()
{
    hotKeyManager()->removeActions();
}

TrayIcon::ActionType TrayIcon::clickEventActionType(IniUser *iniUser, ClickType clickType)
{
    const QString eventName = clickNameByType(clickType);
    const QString actionName = iniUser->trayAction(eventName);

    const ActionType actionType = actionTypeByName(actionName);
    return (actionType != ActionNone) ? actionType : defaultActionTypeByClick(clickType);
}

void TrayIcon::setClickEventActionType(IniUser *iniUser, ClickType clickType, ActionType actionType)
{
    const QString eventName = clickNameByType(clickType);
    const QString actionName = actionNameByType(actionType);

    iniUser->setTrayAction(eventName, actionName);
}

void TrayIcon::updateClickActions()
{
    for (int i = 0; i < ClickTypeCount; ++i) {
        m_clickActions[i] = clickActionFromIni(ClickType(i));
    }
}

QAction *TrayIcon::clickAction(TrayIcon::ClickType clickType) const
{
    return m_clickActions[clickType];
}

QAction *TrayIcon::clickActionFromIni(TrayIcon::ClickType clickType) const
{
    const ActionType actionType = clickEventActionType(iniUser(), clickType);

    return clickActionByType(actionType);
}

QAction *TrayIcon::clickActionByType(TrayIcon::ActionType actionType) const
{
    QAction *actions[] = {
        m_homeAction,
        m_programsAction,
        m_optionsAction,
        m_statisticsAction,
        m_graphAction,
        m_filterEnabledAction,
        m_stopTrafficAction,
        m_stopInetTrafficAction,
        m_filterModeMenuAction,
        m_trayMenuAction,
    };

    if (actionType > TrayIcon::ActionNone && actionType < TrayIcon::ActionIgnore) {
        return actions[actionType];
    }

    return nullptr;
}

void TrayIcon::onMouseClicked(TrayIcon::ClickType clickType)
{
    QAction *action = clickAction(clickType);
    if (action) {
        action->trigger();
    }
}

void TrayIcon::onTrayActivatedByTrigger()
{
    const Qt::KeyboardModifiers kbMods = qApp->queryKeyboardModifiers();
    const ClickType clickType = (kbMods & Qt::ControlModifier) != 0
            ? CtrlSingleClick
            : ((kbMods & Qt::AltModifier) != 0 ? AltSingleClick : SingleClick);

    if (clickAction(DoubleClick)) {
        m_trayTriggered = true;
        QTimer::singleShot(QApplication::doubleClickInterval(), this,
                [&] { onTrayActivatedByClick(clickType, /*checkTriggered=*/true); });
    } else {
        onTrayActivatedByClick(clickType);
    }
}

void TrayIcon::onTrayActivatedByClick(TrayIcon::ClickType clickType, bool checkTriggered)
{
    if (checkTriggered && !m_trayTriggered)
        return;

    m_trayTriggered = false;
    onMouseClicked(clickType);
}
