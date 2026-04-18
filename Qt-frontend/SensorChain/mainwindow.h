#pragma once
#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QFrame>
#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QJsonObject>
#include "apiclient.h"

// Forward-declare all page classes
class DashboardPage;
class DevicesPage;
class SensorsPage;
class AdminPage;
class HistoryPage;
class BlockchainExplorerPage;   // NEW
class ConsensusAnalysisPage;    // NEW

/**
 * @class TxFeedbackDialog
 *
 * Modal dialog shown after every write operation.
 * Displays the enriched transaction receipt so users can see
 * exactly which block the tx was included in, how much gas it used,
 * and what its status is — with a link they can copy into the
 * Blockchain Explorer tab to inspect further.
 */
class TxFeedbackDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TxFeedbackDialog(const QString &operation,
                               const QJsonObject &txData,
                               QWidget *parent = nullptr);
};

// ============================================================

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /**
     * @brief Show a transaction feedback dialog.
     *
     * Connect any write signal to this slot; the dialog summarises
     * the enriched receipt returned by the backend.
     *
     * @param operation  Human-readable name, e.g. "Register Device".
     * @param txData     QJsonObject with enriched receipt fields.
     */
    void showTxFeedback(const QString &operation, const QJsonObject &txData);

private slots:
    void onNavItemChanged(int index);

    // ── Global write-result feedback slots ───────────────────
    void onDeviceRegistered(QJsonObject txData);
    void onFirmwareUpdated(QJsonObject txData);
    void onDeviceDeactivated(QJsonObject txData);
    void onDeviceReactivated(QJsonObject txData);
    void onReadingSubmitted(QJsonObject txData);
    void onNewRoundStarted(QJsonObject txData);
    void onConsensusForced(QJsonObject txData);
    void onFaultyThresholdSet(QJsonObject txData);
    void onMinSensorsSet(QJsonObject txData);
    void onConsensusWindowSet(QJsonObject txData);
    void onDeviceRegistrySet(QJsonObject txData);

private:
    void setupUi();
    void setupStyles();
    void setupGlobalConnections();
    void addNavItem(const QString &icon, const QString &label);

    ApiClient       *m_api;
    QWidget         *m_centralWidget;
    QHBoxLayout     *m_mainLayout;

    // Side nav
    QWidget         *m_navPanel;
    QVBoxLayout     *m_navLayout;
    QLabel          *m_logoLabel;
    QListWidget     *m_navList;
    QLabel          *m_versionLabel;

    // Pages
    QStackedWidget       *m_pages;
    DashboardPage        *m_dashboardPage;
    DevicesPage          *m_devicesPage;
    SensorsPage          *m_sensorsPage;
    AdminPage            *m_adminPage;
    HistoryPage          *m_historyPage;
    BlockchainExplorerPage *m_blockchainPage;   // NEW (index 5)
    ConsensusAnalysisPage  *m_analysisPage;     // NEW (index 6)
};
