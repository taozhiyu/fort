#ifndef APPLISTMODEL_H
#define APPLISTMODEL_H

#include <QDateTime>

#include <sqlite/sqlitetypes.h>

#include <conf/app.h>
#include <util/model/tablesqlmodel.h>

class AppGroup;
class AppInfoCache;
class ConfManager;
class FirewallConf;

struct AppRow : TableRow, public App
{
};

class AppListModel : public TableSqlModel
{
    Q_OBJECT

public:
    explicit AppListModel(QObject *parent = nullptr);

    ConfManager *confManager() const;
    FirewallConf *conf() const;
    AppInfoCache *appInfoCache() const;
    SqliteDb *sqliteDb() const override;

    void initialize();

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant headerData(
            int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    const AppRow &appRowAt(int row) const;
    AppRow appRowById(qint64 appId) const;
    AppRow appRowByPath(const QString &appPath) const;

protected:
    bool updateTableRow(int row) const override;
    TableRow &tableRow() const override { return m_appRow; }

    QString sqlBase() const override;
    QString sqlOrderColumn() const override;

private:
    QVariant headerDataDisplay(int section) const;

    QVariant dataDisplay(const QModelIndex &index) const;
    QVariant dataDisplayState(const AppRow &appRow) const;
    QVariant dataDecoration(const QModelIndex &index) const;
    QVariant dataForeground(const QModelIndex &index) const;
    QVariant dataTextAlignment(const QModelIndex &index) const;

    QVariant appGroupName(const AppRow &appRow) const;
    QVariant appGroupColor(const AppRow &appRow) const;

    static QString appStateText(const AppRow &appRow);
    static QColor appStateColor(const AppRow &appRow);
    static QIcon appStateIcon(const AppRow &appRow);

    bool updateAppRow(const QString &sql, const QVariantList &vars, AppRow &appRow) const;

private:
    mutable AppRow m_appRow;
};

#endif // APPLISTMODEL_H
