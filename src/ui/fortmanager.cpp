#include "fortmanager.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QThreadPool>

#include <fort_version.h>

#include <appinfo/appinfocache.h>
#include <conf/firewallconf.h>
#include <control/controlmanager.h>
#include <form/dialog/passworddialog.h>
#include <fortsettings.h>
#include <hostinfo/hostinfocache.h>
#include <manager/envmanager.h>
#include <manager/hotkeymanager.h>
#include <manager/logger.h>
#include <manager/nativeeventfilter.h>
#include <manager/servicemanager.h>
#include <manager/translationmanager.h>
#include <model/zonelistmodel.h>
#include <rpc/appinfomanagerrpc.h>
#include <rpc/askpendingmanagerrpc.h>
#include <rpc/confmanagerrpc.h>
#include <rpc/drivermanagerrpc.h>
#include <rpc/logmanagerrpc.h>
#include <rpc/quotamanagerrpc.h>
#include <rpc/rpcmanager.h>
#include <rpc/serviceinfomanagerrpc.h>
#include <rpc/statblockmanagerrpc.h>
#include <rpc/statmanagerrpc.h>
#include <rpc/taskmanagerrpc.h>
#include <rpc/windowmanagerfake.h>
#include <task/taskinfozonedownloader.h>
#include <user/usersettings.h>
#include <util/dateutil.h>
#include <util/fileutil.h>
#include <util/ioc/ioccontainer.h>
#include <util/osutil.h>
#include <util/startuputil.h>

namespace {
const QLoggingCategory LC("fortManager");
}

FortManager::FortManager(QObject *parent) : QObject(parent), m_initialized(false) { }

FortManager::~FortManager()
{
    if (m_initialized) {
        closeDriver();
    }

    deleteManagers();

    OsUtil::closeMutex(m_instanceMutex);
}

bool FortManager::checkRunningInstance(bool isService)
{
    bool isSingleInstance;
    m_instanceMutex =
            OsUtil::createMutex(isService ? "Global\\" APP_BASE : APP_BASE, isSingleInstance);
    if (isSingleInstance)
        return true;

    if (isService) {
        qCWarning(LC) << "Quit due Service is already running!";
    } else {
        if (!IoC<ControlManager>()->postCommand(Control::Prog, { "show" })) {
            QMessageBox::warning(nullptr, QString(), tr("Application is already running!"));
        }
    }
    return false;
}

void FortManager::initialize()
{
    m_initialized = true;

    setupThreadPool();
    setupLogger();

    createManagers();

    setupEnvManager();
    setupConfManager();
    setupQuotaManager();
    setupTaskManager();

    setupDriver();
    loadConf();
}

void FortManager::setupThreadPool()
{
    QThreadPool::globalInstance()->setMaxThreadCount(16);
}

void FortManager::setupLogger()
{
    Logger *logger = Logger::instance();

    const auto settings = IoC<FortSettings>();

    logger->setIsService(settings->isService());
    logger->setPath(settings->logsPath());
}

void FortManager::updateLogger(const FirewallConf *conf)
{
    if (!conf->iniEdited())
        return;

    Logger *logger = Logger::instance();

    logger->setDebug(conf->ini().logDebug());
    logger->setConsole(conf->ini().logConsole());
}

void FortManager::createManagers()
{
    IocContainer *ioc = IoC();

    const auto settings = IoC<FortSettings>();

    if (settings->hasService()) {
        ioc->setService(new RpcManager());
    }

    ConfManager *confManager;
    QuotaManager *quotaManager;
    StatManager *statManager;
    StatBlockManager *statBlockManager;
    AskPendingManager *pendingManager;
    DriverManager *driverManager;
    AppInfoManager *appInfoManager;
    LogManager *logManager;
    ServiceInfoManager *serviceInfoManager;
    TaskManager *taskManager;
    WindowManager *windowManager;

    if (settings->isMaster()) {
        // TODO: COMPAT: Remove after v4.1.0 (via v4.0.0)
        FileUtil::copyFile(settings->statFilePath(), settings->statBlockFilePath());

        confManager = new ConfManager(settings->confFilePath());
        quotaManager = new QuotaManager();
        statManager = new StatManager(settings->statFilePath());
        statBlockManager = new StatBlockManager(settings->statBlockFilePath());
        pendingManager = new AskPendingManager();
        driverManager = new DriverManager();
        appInfoManager = new AppInfoManager(settings->cacheFilePath());
        logManager = new LogManager();
        serviceInfoManager = new ServiceInfoManager();
        taskManager = new TaskManager();
    } else {
        confManager = new ConfManagerRpc(settings->confFilePath());
        quotaManager = new QuotaManagerRpc();
        statManager = new StatManagerRpc(settings->statFilePath());
        statBlockManager = new StatBlockManagerRpc(settings->statBlockFilePath());
        pendingManager = new AskPendingManagerRpc();
        driverManager = new DriverManagerRpc();
        appInfoManager = new AppInfoManagerRpc(settings->cacheFilePath());
        logManager = new LogManagerRpc();
        serviceInfoManager = new ServiceInfoManagerRpc();
        taskManager = new TaskManagerRpc();
    }

    if (settings->isService()) {
        windowManager = new WindowManagerFake();

        // For Service only
        ioc->setService(new ServiceManager());
    } else {
        windowManager = new WindowManager();

        // For UI only
        ioc->setService(new HotKeyManager());
        ioc->setService(new UserSettings());
        ioc->setService(new TranslationManager());
    }

    ioc->setService(confManager);
    ioc->setService(quotaManager);
    ioc->setService(statManager);
    ioc->setService(statBlockManager);
    ioc->setService(pendingManager);
    ioc->setService(driverManager);
    ioc->setService(appInfoManager);
    ioc->setService(logManager);
    ioc->setService(serviceInfoManager);
    ioc->setService(taskManager);
    ioc->setService(windowManager);

    ioc->setService(new NativeEventFilter());
    ioc->setService(new AppInfoCache());
    ioc->setService(new HostInfoCache());
    ioc->setService(new ZoneListModel());

    ioc->setUpAll();
}

void FortManager::deleteManagers()
{
    IocContainer *ioc = IoC();

    ioc->tearDownAll();
    ioc->autoDeleteAll();
}

bool FortManager::installDriver()
{
    closeDriver();

    IoC<DriverManager>()->reinstallDriver();

    if (IoC<FortSettings>()->hasService()) {
        // Re-install the service and app restart required to continue
        StartupUtil::setServiceInstalled(true);
        processRestartRequired();
    } else {
        // Re-open the driver device and initialize it
        if (setupDriver()) {
            updateDriverConf();
        }
    }

    return true;
}

bool FortManager::removeDriver()
{
    closeDriver();

    IoC<DriverManager>()->uninstallDriver();

    return true;
}

bool FortManager::setupDriver()
{
    auto driverManager = IoC<DriverManager>();
    auto confManager = IoC<ConfManager>();

    bool ok = driverManager->openDevice();

    if (ok && !confManager->validateDriver()) {
        driverManager->closeDevice();
        ok = false;
    }

    if (ok) {
        confManager->updateDriverServices();
    }

    return ok;
}

void FortManager::closeDriver()
{
    updateLogManager(false);
    updateStatManager(nullptr);

    IoC<DriverManager>()->closeDevice();

    QCoreApplication::sendPostedEvents(this);
}

void FortManager::setupEnvManager()
{
    auto envManager = IoC<EnvManager>();

    connect(IoC<NativeEventFilter>(), &NativeEventFilter::environmentChanged, envManager,
            &EnvManager::onEnvironmentChanged);

    connect(envManager, &EnvManager::environmentUpdated, this, [&] { updateDriverConf(); });
}

void FortManager::setupConfManager()
{
    connect(IoC<ConfManager>(), &ConfManager::confChanged, this, [&](bool onlyFlags) {
        const FirewallConf *conf = IoC<ConfManager>()->conf();

        updateLogger(conf);

        if (!onlyFlags || conf->flagsEdited()) {
            updateDriverConf(onlyFlags);
        }
    });
}

void FortManager::setupQuotaManager()
{
    connect(IoC<QuotaManager>(), &QuotaManager::alert, this, [&](qint8 alertType) {
        IoC<WindowManager>()->showInfoBox(
                QuotaManager::alertTypeText(alertType), tr("Quota Alert"));
    });
}

void FortManager::setupTaskManager()
{
    auto taskManager = IoC<TaskManager>();

    connect(taskManager, &TaskManager::taskDoubleClicked, this, [&](qint8 taskType) {
        if (taskType == TaskInfo::ZoneDownloader) {
            IoC<WindowManager>()->showZonesWindow();
        }
    });
    connect(taskManager->taskInfoZoneDownloader(), &TaskInfoZoneDownloader::zonesUpdated,
            IoC<ConfManager>(), &ConfManager::updateDriverZones);
}

void FortManager::setupTranslationManager()
{
    IoC<TranslationManager>()->switchLanguageByName(IoC<UserSettings>()->iniUser().language());
}

void FortManager::show()
{
    auto windowManager = IoC<WindowManager>();
    const IniUser &iniUser = IoC<UserSettings>()->iniUser();

    setupTranslationManager();

    windowManager->setupTrayIcon();

    if (iniUser.trayShowIcon()) {
        windowManager->showTrayIcon();
    } else {
        windowManager->setupHomeWindow(/*quitOnClose=*/true);
        windowManager->showHomeWindow();
    }

    if (iniUser.graphWindowVisible()) {
        windowManager->showGraphWindow();
    }
}

void FortManager::processRestartRequired()
{
    auto windowManager = IoC<WindowManager>();

    windowManager->showConfirmBox(
            [=] { windowManager->restart(); }, tr("Restart Now?"), tr("Restart Required"));
}

void FortManager::loadConf()
{
    const auto settings = IoC<FortSettings>();

    QString viaVersion;
    if (!settings->canMigrate(viaVersion)) {
        QMessageBox::warning(nullptr, QString(),
                tr("Please first install Fort Firewall v%1 and save Options from it.")
                        .arg(viaVersion));
        abort(); // Abort the program
    }

    IoC<ConfManager>()->load();

    qCDebug(LC) << "Started as"
                << (settings->isService()                   ? "Service"
                                   : settings->hasService() ? "Service Client"
                                                            : "Program");
}

bool FortManager::updateDriverConf(bool onlyFlags)
{
    auto confManager = IoC<ConfManager>();

    updateLogManager(false);

    const bool res = confManager->updateDriverConf(onlyFlags);
    if (res) {
        updateStatManager(confManager->conf());
    }

    updateLogManager(true);

    return res;
}

void FortManager::updateLogManager(bool active)
{
    IoC<LogManager>()->setActive(active);
}

void FortManager::updateStatManager(FirewallConf *conf)
{
    IoC<StatManager>()->setConf(conf);
}

void FortManager::setupPortableResource()
{
    Q_INIT_RESOURCE(fort_readme);
}

void FortManager::setupResources()
{
    Q_INIT_RESOURCE(appinfo_migrations);
    Q_INIT_RESOURCE(conf_migrations);
    Q_INIT_RESOURCE(conf_zone);
    Q_INIT_RESOURCE(stat_migrations);

    Q_INIT_RESOURCE(fort_icons);
}
