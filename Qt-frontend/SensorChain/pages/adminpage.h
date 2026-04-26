#ifndef ADMINPAGE_H
#define ADMINPAGE_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QTextEdit>
#include <QTabWidget>
#include "../apiclient.h"

class AdminPage : public QWidget
{
    Q_OBJECT

public:
    explicit AdminPage(ApiClient *api, QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void onDeviceRegistered(QJsonObject receipt);
    void onFirmwareUpdated(QJsonObject receipt);
    void onDeviceDeactivated(QJsonObject receipt);
    void onDeviceReactivated(QJsonObject receipt);

    void onNewRoundStarted(QJsonObject receipt);
    void onConsensusForced(QJsonObject receipt);
    void onFaultyThresholdSet(QJsonObject receipt);
    void onMinSensorsSet(QJsonObject receipt);
    void onConsensusWindowSet(QJsonObject receipt);
    void onDeviceRegistrySet(QJsonObject receipt);

    void onRegistryStats(QJsonObject data);
    void onConsensusStats(QJsonObject data);

    void onError(QString endpoint, QString error);

    void onRegisterDeviceClicked();
    void onUpdateFirmwareClicked();
    void onDeactivateClicked();
    void onReactivateClicked();
    void onForceNewRoundClicked();
    void onForceConsensusClicked();
    void onSetThresholdClicked();
    void onSetMinSensorsClicked();
    void onSetWindowClicked();
    void onSetRegistryClicked();

private:
    void setupUi();
    QWidget *buildRegistryTab();
    QWidget *buildConsensusTab();
    QWidget *buildInfoTab();
    void logResult(const QString &action, const QJsonObject &receipt, bool success = true);

    ApiClient  *m_api;
    QTabWidget *m_tabs;

    //  Registry tab 
    QLineEdit *m_regAddressInput;
    QLineEdit *m_regFirmwareInput;
    QSpinBox  *m_regVersionInput;
    QLineEdit *m_regTypeInput;
    QPushButton *m_registerBtn;

    QLineEdit *m_updAddressInput;
    QLineEdit *m_updFirmwareInput;
    QSpinBox  *m_updVersionInput;
    QPushButton *m_updateFirmwareBtn;

    QLineEdit *m_deactAddressInput;
    QPushButton *m_deactivateBtn;
    QPushButton *m_reactivateBtn;

    //  Consensus tab 
    QPushButton *m_forceNewRoundBtn;
    QPushButton *m_forceConsensusBtn;

    QSpinBox  *m_thresholdInput;
    QPushButton *m_setThresholdBtn;

    QSpinBox  *m_minSensorsInput;
    QPushButton *m_setMinSensorsBtn;

    QSpinBox  *m_windowInput;
    QPushButton *m_setWindowBtn;

    QLineEdit *m_registryAddressInput;
    QPushButton *m_setRegistryBtn;

    //  Info tab 
    QLabel *m_registryOwnerLabel;
    QLabel *m_registryAddressLabel;
    QLabel *m_totalDevicesLabel;
    QLabel *m_consensusOwnerLabel;
    QLabel *m_consensusAddressLabel;
    QLabel *m_consensusRegistryLabel;
    QLabel *m_currentRoundIdLabel;
    QLabel *m_totalRoundsLabel;
    QLabel *m_minSensorsLabel;
    QLabel *m_thresholdLabel;
    QLabel *m_windowLabel;

    //  Activity log (shared across tabs) 
    QTextEdit *m_activityLog;
};

#endif
