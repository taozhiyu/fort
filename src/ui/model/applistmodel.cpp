#include "applistmodel.h"

#include <QFont>
#include <QIcon>

#include <sqlite/sqlitedb.h>
#include <sqlite/sqlitestmt.h>

#include <appinfo/appinfocache.h>
#include <conf/appgroup.h>
#include <conf/confmanager.h>
#include <conf/firewallconf.h>
#include <util/dateutil.h>
#include <util/fileutil.h>
#include <util/guiutil.h>
#include <util/iconcache.h>
#include <util/ioc/ioccontainer.h>
#include <util/net/netutil.h>

namespace {

const auto alertColor = QColor("orange");
const auto allowColor = QColor("green");
const auto blockColor = QColor("red");
const auto killProcessColor = QColor("magenta");
const auto inactiveColor = QColor("slategray");

QString appStateIconPath(const AppRow &appRow)
{
    if (appRow.alerted)
        return ":/icons/error.png";

    if (appRow.killProcess)
        return ":/icons/scull.png";

    if (appRow.blocked)
        return ":/icons/deny.png";

    if (!appRow.endTime.isNull())
        return ":/icons/time.png";

    if (appRow.lanOnly)
        return ":/icons/hostname.png";

    return ":/icons/accept.png";
}

}

AppListModel::AppListModel(QObject *parent) : TableSqlModel(parent) { }

ConfManager *AppListModel::confManager() const
{
    return IoC<ConfManager>();
}

FirewallConf *AppListModel::conf() const
{
    return confManager()->conf();
}

AppInfoCache *AppListModel::appInfoCache() const
{
    return IoC<AppInfoCache>();
}

SqliteDb *AppListModel::sqliteDb() const
{
    return confManager()->sqliteDb();
}

void AppListModel::initialize()
{
    setSortColumn(5);
    setSortOrder(Qt::DescendingOrder);

    connect(confManager(), &ConfManager::confChanged, this, [&](bool onlyFlags = false) {
        if (!onlyFlags) {
            refresh();
        }
    });
    connect(confManager(), &ConfManager::appChanged, this, &TableSqlModel::reset);
    connect(confManager(), &ConfManager::appUpdated, this, &TableSqlModel::refresh);

    connect(appInfoCache(), &AppInfoCache::cacheChanged, this, &AppListModel::refresh);
}

int AppListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 5;
}

QVariant AppListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && (role == Qt::DisplayRole || role == Qt::ToolTipRole)) {
        return headerDataDisplay(section);
    }
    return QVariant();
}

QVariant AppListModel::headerDataDisplay(int section) const
{
    switch (section) {
    case 0:
        return tr("Name");
    case 1:
        return tr("Action");
    case 2:
        return tr("Group");
    case 3:
        return tr("File Path");
    case 4:
        return tr("Creation Time");
    default:
        Q_UNREACHABLE();
        return QVariant();
    }
}

QVariant AppListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    switch (role) {
    // Label
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return dataDisplay(index);

    // Icon
    case Qt::DecorationRole:
        return dataDecoration(index);

    // Foreground
    case Qt::ForegroundRole:
        return dataForeground(index);

    // Text Alignment
    case Qt::TextAlignmentRole:
        return dataTextAlignment(index);
    }

    return QVariant();
}

QVariant AppListModel::dataDisplay(const QModelIndex &index) const
{
    const int row = index.row();
    const int column = index.column();

    const auto appRow = appRowAt(row);
    if (appRow.isNull())
        return QVariant();

    switch (column) {
    case 0:
        return appRow.appName;
    case 1:
        return dataDisplayState(appRow);
    case 2:
        return appGroupName(appRow);
    case 3:
        return appRow.appPath;
    case 4:
        return appRow.creatTime;
    }

    return QVariant();
}

QVariant AppListModel::dataDisplayState(const AppRow &appRow) const
{
    QString text = appStateText(appRow);
    if (!appRow.endTime.isNull()) {
        text += QStringLiteral(" (%1)").arg(
                DateUtil::localeDateTime(appRow.endTime, QLocale::ShortFormat));
    }
    return text;
}

QVariant AppListModel::dataDecoration(const QModelIndex &index) const
{
    const int column = index.column();

    if (column == 0 || column == 1) {
        const int row = index.row();

        const auto appRow = appRowAt(row);
        if (appRow.isNull())
            return QVariant();

        switch (column) {
        case 0:
            return appInfoCache()->appIcon(appRow.appPath);
        case 1:
            return appStateIcon(appRow);
        }
    }

    return QVariant();
}

QVariant AppListModel::dataForeground(const QModelIndex &index) const
{
    const int column = index.column();

    if (column == 1 || column == 2) {
        const int row = index.row();
        const auto appRow = appRowAt(row);

        switch (column) {
        case 1:
            return appStateColor(appRow);
        case 2:
            return appGroupColor(appRow);
        }
    }

    return QVariant();
}

QVariant AppListModel::dataTextAlignment(const QModelIndex &index) const
{
    const int column = index.column();

    if (column == 2) {
        return int(Qt::AlignHCenter | Qt::AlignVCenter);
    }

    return QVariant();
}

QVariant AppListModel::appGroupName(const AppRow &appRow) const
{
    const AppGroup *appGroup = conf()->appGroupAt(appRow.groupIndex);

    return appGroup->name();
}

QVariant AppListModel::appGroupColor(const AppRow &appRow) const
{
    if (!appRow.useGroupPerm)
        return inactiveColor;

    const AppGroup *appGroup = conf()->appGroupAt(appRow.groupIndex);
    if (!appGroup->enabled())
        return blockColor;

    return {};
}

QString AppListModel::appStateText(const AppRow &appRow)
{
    if (appRow.killProcess)
        return tr("Kill Process");

    if (appRow.blocked)
        return tr("Block");

    return tr("Allow");
}

QColor AppListModel::appStateColor(const AppRow &appRow)
{
    if (appRow.killProcess)
        return killProcessColor;

    if (appRow.blocked)
        return blockColor;

    return allowColor;
}

QIcon AppListModel::appStateIcon(const AppRow &appRow)
{
    return IconCache::icon(appStateIconPath(appRow));
}

bool AppListModel::updateAppRow(const QString &sql, const QVariantList &vars, AppRow &appRow) const
{
    SqliteStmt stmt;
    if (!(sqliteDb()->prepare(stmt, sql, vars) && stmt.step() == SqliteStmt::StepRow)) {
        appRow.invalidate();
        return false;
    }

    appRow.appId = stmt.columnInt64(0);
    appRow.groupIndex = stmt.columnInt(1);
    appRow.appPath = stmt.columnText(2);
    appRow.appName = stmt.columnText(3);
    appRow.useGroupPerm = stmt.columnBool(4);
    appRow.applyChild = stmt.columnBool(5);
    appRow.lanOnly = stmt.columnBool(6);
    appRow.logBlocked = stmt.columnBool(7);
    appRow.logConn = stmt.columnBool(8);
    appRow.blocked = stmt.columnBool(9);
    appRow.killProcess = stmt.columnBool(10);
    appRow.alerted = stmt.columnBool(11);
    appRow.endTime = stmt.columnDateTime(12);
    appRow.creatTime = stmt.columnDateTime(13);

    return true;
}

const AppRow &AppListModel::appRowAt(int row) const
{
    updateRowCache(row);

    return m_appRow;
}

AppRow AppListModel::appRowById(qint64 appId) const
{
    AppRow appRow;
    updateAppRow(sqlBase() + " WHERE t.app_id = ?1;", { appId }, appRow);
    return appRow;
}

AppRow AppListModel::appRowByPath(const QString &appPath) const
{
    AppRow appRow;
    if (!updateAppRow(sqlBase() + " WHERE t.path = ?1;", { appPath }, appRow)) {
        appRow.appPath = appPath;
    }
    return appRow;
}

bool AppListModel::updateTableRow(int row) const
{
    return updateAppRow(sql(), { row }, m_appRow);
}

QString AppListModel::sqlBase() const
{
    return "SELECT"
           "    t.app_id,"
           "    g.order_index as group_index,"
           "    t.path,"
           "    t.name,"
           "    t.use_group_perm,"
           "    t.apply_child,"
           "    t.lan_only,"
           "    t.log_blocked,"
           "    t.log_conn,"
           "    t.blocked,"
           "    t.kill_process,"
           "    (alert.app_id IS NOT NULL) as alerted,"
           "    t.end_time,"
           "    t.creat_time"
           "  FROM app t"
           "    JOIN app_group g ON g.app_group_id = t.app_group_id"
           "    LEFT JOIN app_alert alert ON alert.app_id = t.app_id";
}

QString AppListModel::sqlOrderColumn() const
{
    QString columnsStr;
    switch (sortColumn()) {
    case 0: // Name
        columnsStr = "4 " + sqlOrderAsc() + ", 3";
        break;
    case 1: // State
        columnsStr = "12 DESC, 11, 10 " + sqlOrderAsc() + ", 1";
        break;
    case 2: // Group
        columnsStr = "2";
        break;
    case 3: // File Path
        columnsStr = "3";
        break;
    default: // Creation Time
        columnsStr = "1"; // App ID
        break;
    }

    return columnsStr;
}
