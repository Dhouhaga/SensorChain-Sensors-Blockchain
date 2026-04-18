#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTableWidget>
#include <QTabWidget>
#include <QSplitter>
#include <QJsonObject>
#include <QJsonArray>
#include "../apiclient.h"

/**
 * @class BlockchainExplorerPage
 *
 * Provides three tabs:
 *   1. Blocks  — latest block summary + block-by-number lookup
 *   2. Transactions — tx hash lookup + decoded log viewer
 *   3. Events  — unified event table (all contracts) with
 *                full event → tx → block traceability
 */
class BlockchainExplorerPage : public QWidget
{
    Q_OBJECT

public:
    explicit BlockchainExplorerPage(ApiClient *api, QWidget *parent = nullptr);

    /** Called by MainWindow when the tab is activated. */
    void refresh();

private slots:
    // Network / latest block
    void onNetworkReady(QJsonObject data);
    void onLatestBlockReady(QJsonObject data);

    // Block lookup
    void onFetchBlock();
    void onBlockReady(QJsonObject data);

    // Tx lookup
    void onFetchTx();
    void onTxReady(QJsonObject data);

    // Events
    void onRefreshEvents();
    void onEventHistoryReady(QJsonObject data);

    // Error
    void onError(const QString &endpoint, const QString &msg);

private:
    void setupUi();
    void setupConnections();

    // ── helpers ─────────────────────────────────────────────
    /** Render a QJsonObject as indented JSON in a QTextEdit. */
    static void showJson(QTextEdit *te, const QJsonObject &obj);
    /** Append a row to the events table. */
    void appendEventRow(const QString &event,
                        const QString &summary,
                        const QString &txHash,
                        int blockNumber,
                        const QString &timestamp);

    ApiClient *m_api;

    // ── Tabs ─────────────────────────────────────────────────
    QTabWidget *m_tabs;

    // ── Tab 1: Blocks ────────────────────────────────────────
    QLabel     *m_networkLabel;
    QLabel     *m_latestBlockNum;
    QLabel     *m_latestBlockHash;
    QLabel     *m_latestBlockTs;
    QLabel     *m_latestBlockTxCount;
    QLabel     *m_gasPriceLabel;

    QLineEdit  *m_blockNumberInput;
    QPushButton *m_fetchBlockBtn;
    QTextEdit  *m_blockDetail;

    // ── Tab 2: Transactions ──────────────────────────────────
    QLineEdit  *m_txHashInput;
    QPushButton *m_fetchTxBtn;
    QTextEdit  *m_txDetail;

    // ── Tab 3: Events ────────────────────────────────────────
    QPushButton *m_refreshEventsBtn;
    QTableWidget *m_eventsTable;
};
