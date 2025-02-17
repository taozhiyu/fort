#ifndef OPTIONSPAGE_H
#define OPTIONSPAGE_H

#include "optbasepage.h"

class OptionsPage : public OptBasePage
{
    Q_OBJECT

public:
    explicit OptionsPage(OptionsController *ctrl = nullptr, QWidget *parent = nullptr);

    bool passwordEdited() const { return m_passwordEdited; }
    void setPasswordEdited(bool v) { m_passwordEdited = v; }

    bool languageEdited() const { return m_languageEdited; }
    void setLanguageEdited(bool v) { m_languageEdited = v; }

protected slots:
    void onAboutToSave() override;
    void onEditResetted() override;

    void onRetranslateUi() override;

private:
    void saveAutoRunMode(int mode);
    void saveService(bool isService);

    void retranslateComboStartMode();
    void retranslateComboFilterMode();
    void retranslateEditPassword();
    void retranslateComboTrayEvent();
    void retranslateComboTrayAction();
    void retranslateDriverMessage();

    void setupStartup();

    void setupUi();
    QLayout *setupColumn1();
    void setupStartupBox();
    QLayout *setupStartModeLayout();
    void setupTrafficBox();
    QLayout *setupFilterModeLayout();
    void setupGlobalBox();
    void setupHotKeysBox();
    QLayout *setupLangLayout();
    void setupComboLanguage();
    void setupProtectionBox();
    QLayout *setupPasswordLayout();
    void setupEditPassword();
    void setupPasswordLock();
    void setupTrayBox();
    void refreshComboTrayAction();
    QLayout *setupTrayEventLayout();
    QLayout *setupTrayActionLayout();
    void setupConfirmationsBox();
    void setupLogsBox();
    QLayout *setupColumn2();
    void setupDriverBox();
    void setupDriverIcon();
    void setupNewVersionBox();
    void setupNewVersionUpdate();

private:
    bool m_passwordEdited : 1;
    bool m_languageEdited : 1;

    qint8 m_currentAutoRunMode = 0;

    QGroupBox *m_gbStartup = nullptr;
    QGroupBox *m_gbTraffic = nullptr;
    QGroupBox *m_gbGlobal = nullptr;
    QGroupBox *m_gbHotKeys = nullptr;
    QGroupBox *m_gbProtection = nullptr;
    QGroupBox *m_gbTray = nullptr;
    QGroupBox *m_gbConfirmations = nullptr;
    QGroupBox *m_gbLogs = nullptr;
    QGroupBox *m_gbDriver = nullptr;
    QGroupBox *m_gbNewVersion = nullptr;
    QLabel *m_labelStartMode = nullptr;
    QComboBox *m_comboAutoRun = nullptr;
    QCheckBox *m_cbService = nullptr;
    QCheckBox *m_cbFilterEnabled = nullptr;
    QCheckBox *m_cbStopTraffic = nullptr;
    QCheckBox *m_cbStopInetTraffic = nullptr;
    QLabel *m_labelFilterMode = nullptr;
    QComboBox *m_comboFilterMode = nullptr;
    QCheckBox *m_cbExplorerMenu = nullptr;
    QCheckBox *m_cbHotKeysEnabled = nullptr;
    QCheckBox *m_cbHotKeysGlobal = nullptr;
    QLabel *m_labelLanguage = nullptr;
    QComboBox *m_comboLanguage = nullptr;
    QCheckBox *m_cbBootFilter = nullptr;
    QCheckBox *m_cbFilterLocals = nullptr;
    QCheckBox *m_cbNoServiceControl = nullptr;
    QCheckBox *m_cbCheckPasswordOnUninstall = nullptr;
    QCheckBox *m_cbPassword = nullptr;
    QLineEdit *m_editPassword = nullptr;
    QToolButton *m_btPasswordLock = nullptr;
    QCheckBox *m_cbTrayShowIcon = nullptr;
    QCheckBox *m_cbTrayAnimateAlert = nullptr;
    QLabel *m_labelTrayEvent = nullptr;
    QComboBox *m_comboTrayEvent = nullptr;
    QLabel *m_labelTrayAction = nullptr;
    QComboBox *m_comboTrayAction = nullptr;
    QCheckBox *m_cbConfirmTrayFlags = nullptr;
    QCheckBox *m_cbConfirmQuit = nullptr;
    QCheckBox *m_cbLogDebug = nullptr;
    QCheckBox *m_cbLogConsole = nullptr;
    QLabel *m_iconDriver = nullptr;
    QLabel *m_labelDriverMessage = nullptr;
    QPushButton *m_btInstallDriver = nullptr;
    QPushButton *m_btRemoveDriver = nullptr;
    QLabel *m_labelNewVersion = nullptr;
    QPushButton *m_btNewVersion = nullptr;
};

#endif // OPTIONSPAGE_H
