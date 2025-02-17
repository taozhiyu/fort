#include "appinfocache.h"

#include <QIcon>
#include <QImage>

#include <util/iconcache.h>
#include <util/ioc/ioccontainer.h>

#include "appinfomanager.h"

AppInfoCache::AppInfoCache(QObject *parent) : QObject(parent), m_cache(1000)
{
    connect(&m_triggerTimer, &QTimer::timeout, this, &AppInfoCache::cacheChanged);
}

void AppInfoCache::setUp()
{
    connect(IoC<AppInfoManager>(), &AppInfoManager::lookupInfoFinished, this,
            &AppInfoCache::handleFinishedInfoLookup);
    connect(IoC<AppInfoManager>(), &AppInfoManager::lookupIconFinished, this,
            &AppInfoCache::handleFinishedIconLookup);
}

void AppInfoCache::tearDown()
{
    disconnect(IoC<AppInfoManager>());
}

QString AppInfoCache::appName(const QString &appPath)
{
    AppInfo appInfo = this->appInfo(appPath);
    if (!appInfo.isValid()) {
        IoC<AppInfoManager>()->loadInfoFromFs(appPath, appInfo);
    }
    return appInfo.fileDescription;
}

QIcon AppInfoCache::appIcon(const QString &appPath, const QString &nullIconPath)
{
    QPixmap pixmap;
    if (IconCache::find(appPath, &pixmap))
        return pixmap;

    const auto info = appInfo(appPath);
    if (info.isValid()) {
        IoC<AppInfoManager>()->lookupAppIcon(appPath, info.iconId);
    }

    pixmap = IconCache::file(!nullIconPath.isEmpty() ? nullIconPath : ":/icons/application.png");

    IconCache::insert(appPath, pixmap);

    return pixmap;
}

AppInfo AppInfoCache::appInfo(const QString &appPath)
{
    if (appPath.isEmpty())
        return AppInfo();

    AppInfo *appInfo = m_cache.object(appPath);
    bool lookupRequired = false;

    auto appInfoManager = IoC<AppInfoManager>();

    if (!appInfo) {
        appInfo = new AppInfo();

        m_cache.insert(appPath, appInfo, 1);

        lookupRequired = !appInfoManager->loadInfoFromDb(appPath, *appInfo);
    }

    if (!lookupRequired) {
        lookupRequired = appInfo->isValid() && appInfo->isFileModified(appPath);
    }

    if (lookupRequired) {
        appInfoManager->lookupAppInfo(appPath);
    }

    return *appInfo;
}

void AppInfoCache::handleFinishedInfoLookup(const QString &appPath, const AppInfo &info)
{
    AppInfo *appInfo = m_cache.object(appPath);
    if (!appInfo)
        return;

    *appInfo = info;

    IconCache::remove(appPath); // invalidate cached icon

    emitCacheChanged();
}

void AppInfoCache::handleFinishedIconLookup(const QString &appPath, const QImage &image)
{
    if (image.isNull())
        return;

    IconCache::insert(appPath, QPixmap::fromImage(image)); // update cached icon

    emitCacheChanged();
}

void AppInfoCache::emitCacheChanged()
{
    m_triggerTimer.startTrigger();
}
