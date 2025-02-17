#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>

#include <util/ioc/iocservice.h>

class GraphWindow;
class HomeWindow;
class MainWindow;
class OptionsWindow;
class PoliciesWindow;
class ProgramsWindow;
class ServicesWindow;
class StatisticsWindow;
class TrayIcon;
class ZonesWindow;

class WindowManager : public QObject, public IocService
{
    Q_OBJECT

public:
    enum TrayMessageType { MessageOptions, MessageZones };
    Q_ENUM(TrayMessageType)

    explicit WindowManager(QObject *parent = nullptr);

    MainWindow *mainWindow() const { return m_mainWindow; }
    HomeWindow *homeWindow() const { return m_homeWindow; }
    ProgramsWindow *progWindow() const { return m_progWindow; }
    PoliciesWindow *policiesWindow() const { return m_policiesWindow; }
    OptionsWindow *optWindow() const { return m_optWindow; }
    StatisticsWindow *connWindow() const { return m_statWindow; }
    ServicesWindow *serviceWindow() const { return m_serviceWindow; }
    ZonesWindow *zoneWindow() const { return m_zoneWindow; }
    GraphWindow *graphWindow() const { return m_graphWindow; }
    TrayIcon *trayIcon() const { return m_trayIcon; }

    void setUp() override;
    void tearDown() override;

    static void showWidget(QWidget *w);

    static QFont defaultFont();

signals:
    void optWindowChanged(bool visible);
    void graphWindowChanged(bool visible);

public slots:
    void setupAppPalette();

    void setupTrayIcon();
    void showTrayIcon();
    void closeTrayIcon();
    void showTrayMessage(
            const QString &message, WindowManager::TrayMessageType type = MessageOptions);

    void setupHomeWindow(bool quitOnClose = false);
    void showHomeWindow();
    void closeHomeWindow();

    void showProgramsWindow();
    void closeProgramsWindow();

    bool showProgramEditForm(const QString &appPath);

    void showOptionsWindow();
    void closeOptionsWindow();
    void reloadOptionsWindow(const QString &reason);

    void showPoliciesWindow();
    void closePoliciesWindow();

    void showStatisticsWindow();
    void closeStatisticsWindow();

    void showServicesWindow();
    void closeServicesWindow();

    void showZonesWindow();
    void closeZonesWindow();

    void showGraphWindow();
    void closeGraphWindow(bool wasVisible = false);
    void switchGraphWindow();

    void closeAll();
    void quit();
    void restart();

    bool widgetVisibleByCheckPassword(QWidget *w);
    bool checkPassword();

    virtual void showErrorBox(
            const QString &text, const QString &title = QString(), QWidget *parent = nullptr);
    virtual void showInfoBox(
            const QString &text, const QString &title = QString(), QWidget *parent = nullptr);
    void showConfirmBox(const std::function<void()> &onConfirmed, const QString &text,
            const QString &title = QString(), QWidget *parent = nullptr);
    void showQuestionBox(const std::function<void(bool confirmed)> &onFinished, const QString &text,
            const QString &title = QString(), QWidget *parent = nullptr);

    static bool activateModalWidget();

private:
    void setupMainWindow();
    void closeMainWindow();

    void setupProgramsWindow();
    void setupOptionsWindow();
    void setupPoliciesWindow();
    void setupServicesWindow();
    void setupZonesWindow();
    void setupGraphWindow();
    void setupStatisticsWindow();

    void onTrayMessageClicked();

private:
    TrayMessageType m_lastMessageType = MessageOptions;

    TrayIcon *m_trayIcon = nullptr;

    MainWindow *m_mainWindow = nullptr;
    HomeWindow *m_homeWindow = nullptr;
    ProgramsWindow *m_progWindow = nullptr;
    OptionsWindow *m_optWindow = nullptr;
    PoliciesWindow *m_policiesWindow = nullptr;
    StatisticsWindow *m_statWindow = nullptr;
    ServicesWindow *m_serviceWindow = nullptr;
    ZonesWindow *m_zoneWindow = nullptr;
    GraphWindow *m_graphWindow = nullptr;
};

#endif // WINDOWMANAGER_H
