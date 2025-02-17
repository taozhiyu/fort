#include "optbasepage.h"

#include <QAbstractButton>

#include <conf/firewallconf.h>
#include <form/opt/optionscontroller.h>
#include <fortmanager.h>
#include <user/iniuser.h>
#include <util/osutil.h>

OptBasePage::OptBasePage(OptionsController *ctrl, QWidget *parent) : QFrame(parent), m_ctrl(ctrl)
{
    setupController();
}

FortManager *OptBasePage::fortManager() const
{
    return ctrl()->fortManager();
}

FortSettings *OptBasePage::settings() const
{
    return ctrl()->settings();
}

ConfManager *OptBasePage::confManager() const
{
    return ctrl()->confManager();
}

FirewallConf *OptBasePage::conf() const
{
    return ctrl()->confToEdit();
}

IniOptions *OptBasePage::ini() const
{
    return &conf()->ini();
}

IniUser *OptBasePage::iniUser() const
{
    return ctrl()->iniUserToEdit();
}

DriverManager *OptBasePage::driverManager() const
{
    return ctrl()->driverManager();
}

TranslationManager *OptBasePage::translationManager() const
{
    return ctrl()->translationManager();
}

WindowManager *OptBasePage::windowManager() const
{
    return ctrl()->windowManager();
}

TaskManager *OptBasePage::taskManager() const
{
    return ctrl()->taskManager();
}

ZoneListModel *OptBasePage::zoneListModel() const
{
    return ctrl()->zoneListModel();
}

void OptBasePage::setupController()
{
    Q_ASSERT(ctrl());

    connect(ctrl(), &OptionsController::aboutToSave, this, &OptBasePage::onAboutToSave);
    connect(ctrl(), &OptionsController::editResetted, this, &OptBasePage::onEditResetted);

    connect(ctrl(), &OptionsController::afterSaveWindowState, this,
            &OptBasePage::onSaveWindowState);
    connect(ctrl(), &OptionsController::afterRestoreWindowState, this,
            &OptBasePage::onRestoreWindowState);

    connect(ctrl(), &OptionsController::retranslateUi, this, &OptBasePage::onRetranslateUi);
}

void OptBasePage::onLinkClicked()
{
    auto button = qobject_cast<QAbstractButton *>(sender());
    if (button) {
        OsUtil::openUrlOrFolder(button->windowFilePath());
    }
}
