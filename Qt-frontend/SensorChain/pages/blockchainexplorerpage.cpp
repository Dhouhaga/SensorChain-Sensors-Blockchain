#include "blockchainexplorerpage.h"
#include <QJsonDocument>
#include <QHeaderView>
#include <QDateTime>
#include <QScrollArea>

// ============================================================
// Constructor
// ============================================================

BlockchainExplorerPage::BlockchainExplorerPage(ApiClient *api, QWidget *parent)
    : QWidget(parent)
    , m_api(api)
{
    setupUi();
    setupConnections();
}

// ============================================================
// Public
// ============================================================

void BlockchainExplorerPage::refresh()
{
    m_api->getBlockchainNetwork();
    m_api->getBlockchainLatest();
}

// ============================================================
// UI setup
// ============================================================

void BlockchainExplorerPage::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    // ── Page title ────────────────────────────────────────────
    auto *titleLabel = new QLabel("⛓ Blockchain Explorer");
    titleLabel->setObjectName("pageTitle");
    auto *subtitleLabel = new QLabel("Inspect blocks, transactions, and contract events");
    subtitleLabel->setObjectName("pageSubtitle");
    root->addWidget(titleLabel);
    root->addWidget(subtitleLabel);

    // ── Tabs ──────────────────────────────────────────────────
    m_tabs = new QTabWidget();
    root->addWidget(m_tabs, 1);

    // ─────────────────────────────────────────────────────────
    // TAB 1: Blocks
    // ─────────────────────────────────────────────────────────
    auto *blocksTab    = new QWidget();
    auto *blocksLayout = new QVBoxLayout(blocksTab);
    blocksLayout->setContentsMargins(16, 16, 16, 16);
    blocksLayout->setSpacing(12);

    // Network info strip
    auto *netBox    = new QGroupBox("Network");
    auto *netLayout = new QHBoxLayout(netBox);
    m_networkLabel  = new QLabel("–");
    m_gasPriceLabel = new QLabel("–");
    netLayout->addWidget(new QLabel("Chain:"));
    netLayout->addWidget(m_networkLabel);
    netLayout->addSpacing(24);
    netLayout->addWidget(new QLabel("Gas price:"));
    netLayout->addWidget(m_gasPriceLabel);
    netLayout->addStretch();
    blocksLayout->addWidget(netBox);

    // Latest block summary cards
    auto *latestBox    = new QGroupBox("Latest Block");
    auto *latestLayout = new QHBoxLayout(latestBox);

    auto makeCard = [](const QString &title, QLabel *&valLabel) -> QWidget* {
        auto *w  = new QWidget();
        auto *vl = new QVBoxLayout(w);
        vl->setContentsMargins(12, 8, 12, 8);
        auto *t  = new QLabel(title);
        t->setObjectName("pageSubtitle");
        valLabel = new QLabel("–");
        valLabel->setObjectName("statValueSmall");
        valLabel->setWordWrap(true);
        vl->addWidget(t);
        vl->addWidget(valLabel);
        return w;
    };

    latestLayout->addWidget(makeCard("Block Number",    m_latestBlockNum));
    latestLayout->addWidget(makeCard("Block Hash",      m_latestBlockHash));
    latestLayout->addWidget(makeCard("Timestamp (UTC)", m_latestBlockTs));
    latestLayout->addWidget(makeCard("Tx Count",        m_latestBlockTxCount));
    blocksLayout->addWidget(latestBox);

    // Block lookup
    auto *lookupBox    = new QGroupBox("Block Lookup");
    auto *lookupLayout = new QVBoxLayout(lookupBox);
    auto *inputRow     = new QHBoxLayout();
    m_blockNumberInput = new QLineEdit();
    m_blockNumberInput->setPlaceholderText("Enter block number…");
    m_fetchBlockBtn    = new QPushButton("Fetch Block");
    m_fetchBlockBtn->setObjectName("primaryBtn");
    inputRow->addWidget(m_blockNumberInput, 1);
    inputRow->addWidget(m_fetchBlockBtn);
    m_blockDetail = new QTextEdit();
    m_blockDetail->setReadOnly(true);
    m_blockDetail->setPlaceholderText("Block details appear here…");
    lookupLayout->addLayout(inputRow);
    lookupLayout->addWidget(m_blockDetail, 1);
    blocksLayout->addWidget(lookupBox, 1);

    m_tabs->addTab(blocksTab, "🔲 Blocks");

    // ─────────────────────────────────────────────────────────
    // TAB 2: Transactions
    // ─────────────────────────────────────────────────────────
    auto *txTab    = new QWidget();
    auto *txLayout = new QVBoxLayout(txTab);
    txLayout->setContentsMargins(16, 16, 16, 16);
    txLayout->setSpacing(12);

    auto *txBox    = new QGroupBox("Transaction Inspector");
    auto *txBoxLay = new QVBoxLayout(txBox);
    auto *txRow    = new QHBoxLayout();
    m_txHashInput  = new QLineEdit();
    m_txHashInput->setPlaceholderText("Enter transaction hash (0x…)");
    m_fetchTxBtn   = new QPushButton("Fetch Tx");
    m_fetchTxBtn->setObjectName("primaryBtn");
    txRow->addWidget(m_txHashInput, 1);
    txRow->addWidget(m_fetchTxBtn);
    m_txDetail = new QTextEdit();
    m_txDetail->setReadOnly(true);
    m_txDetail->setPlaceholderText(
        "Paste a tx hash above to inspect the full transaction,\n"
        "receipt, gas details, and decoded event logs.");
    txBoxLay->addLayout(txRow);
    txBoxLay->addWidget(m_txDetail, 1);
    txLayout->addWidget(txBox, 1);

    m_tabs->addTab(txTab, "📄 Transactions");

    // ─────────────────────────────────────────────────────────
    // TAB 3: Events
    // ─────────────────────────────────────────────────────────
    auto *evTab    = new QWidget();
    auto *evLayout = new QVBoxLayout(evTab);
    evLayout->setContentsMargins(16, 16, 16, 16);
    evLayout->setSpacing(8);

    auto *evHeader = new QHBoxLayout();
    auto *evTitle  = new QLabel("All Contract Events");
    evTitle->setObjectName("pageSubtitle");
    m_refreshEventsBtn = new QPushButton("↻ Refresh");
    m_refreshEventsBtn->setObjectName("primaryBtn");
    evHeader->addWidget(evTitle, 1);
    evHeader->addWidget(m_refreshEventsBtn);
    evLayout->addLayout(evHeader);

    m_eventsTable = new QTableWidget(0, 5);
    m_eventsTable->setHorizontalHeaderLabels({"Event", "Summary", "Tx Hash", "Block #", "Timestamp"});
    m_eventsTable->horizontalHeader()->setStretchLastSection(true);
    m_eventsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_eventsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventsTable->verticalHeader()->setVisible(false);
    evLayout->addWidget(m_eventsTable, 1);

    m_tabs->addTab(evTab, "📋 Events");
}

// ============================================================
// Connections
// ============================================================

void BlockchainExplorerPage::setupConnections()
{
    connect(m_api, &ApiClient::blockchainNetworkReady,
            this,  &BlockchainExplorerPage::onNetworkReady);
    connect(m_api, &ApiClient::blockchainLatestReady,
            this,  &BlockchainExplorerPage::onLatestBlockReady);
    connect(m_api, &ApiClient::blockchainBlockReady,
            this,  &BlockchainExplorerPage::onBlockReady);
    connect(m_api, &ApiClient::blockchainTxReady,
            this,  &BlockchainExplorerPage::onTxReady);
    connect(m_api, &ApiClient::eventHistoryReady,
            this,  &BlockchainExplorerPage::onEventHistoryReady);
    connect(m_api, &ApiClient::requestError,
            this,  &BlockchainExplorerPage::onError);

    connect(m_fetchBlockBtn, &QPushButton::clicked,
            this,            &BlockchainExplorerPage::onFetchBlock);
    connect(m_fetchTxBtn,    &QPushButton::clicked,
            this,            &BlockchainExplorerPage::onFetchTx);
    connect(m_refreshEventsBtn, &QPushButton::clicked,
            this,               &BlockchainExplorerPage::onRefreshEvents);
}

// ============================================================
// Slots
// ============================================================

void BlockchainExplorerPage::onNetworkReady(QJsonObject data)
{
    m_networkLabel->setText(
        QString("Chain %1 (%2)  |  RPC: %3")
            .arg(data["chainId"].toInt())
            .arg(data["networkName"].toString())
            .arg(data["rpcUrl"].toString())
    );
    m_gasPriceLabel->setText(
        QString("%1 Gwei").arg(data["gasPriceGwei"].toString())
    );
}

void BlockchainExplorerPage::onLatestBlockReady(QJsonObject data)
{
    m_latestBlockNum->setText(QString::number(data["blockNumber"].toInt()));
    m_latestBlockHash->setText(data["blockHash"].toString().left(18) + "…");
    m_latestBlockTs->setText(data["timestampISO"].toString().replace("T", " ").left(19));
    m_latestBlockTxCount->setText(QString::number(data["transactionCount"].toInt()));
}

void BlockchainExplorerPage::onFetchBlock()
{
    QString text = m_blockNumberInput->text().trimmed();
    if (text.isEmpty()) return;
    bool ok;
    int num = text.toInt(&ok);
    if (!ok) { m_blockDetail->setPlainText("Invalid block number."); return; }
    m_fetchBlockBtn->setEnabled(false);
    m_fetchBlockBtn->setText("Fetching…");
    m_api->getBlockchainBlock(num);
}

void BlockchainExplorerPage::onBlockReady(QJsonObject data)
{
    m_fetchBlockBtn->setEnabled(true);
    m_fetchBlockBtn->setText("Fetch Block");
    showJson(m_blockDetail, data);
}

void BlockchainExplorerPage::onFetchTx()
{
    QString hash = m_txHashInput->text().trimmed();
    if (hash.isEmpty()) return;
    m_fetchTxBtn->setEnabled(false);
    m_fetchTxBtn->setText("Fetching…");
    m_api->getBlockchainTx(hash);
}

void BlockchainExplorerPage::onTxReady(QJsonObject data)
{
    m_fetchTxBtn->setEnabled(true);
    m_fetchTxBtn->setText("Fetch Tx");
    showJson(m_txDetail, data);
}

void BlockchainExplorerPage::onRefreshEvents()
{
    m_refreshEventsBtn->setEnabled(false);
    m_refreshEventsBtn->setText("Loading…");
    m_api->getEventHistory(0);
}

void BlockchainExplorerPage::onEventHistoryReady(QJsonObject data)
{
    m_refreshEventsBtn->setEnabled(true);
    m_refreshEventsBtn->setText("↻ Refresh");
    m_eventsTable->setRowCount(0);

    auto addGroup = [&](const QString &key) {
        QJsonArray arr = data[key].toArray();
        for (const auto &v : arr) {
            QJsonObject e = v.toObject();
            QString event   = e["event"].toString();
            int     block   = e["blockNumber"].toInt();
            QString txHash  = e["txHash"].toString();
            QString ts      = e["blockTimestampISO"].isUndefined()
                                ? e["timestamp"].toString()
                                : e["blockTimestampISO"].toString().left(19).replace("T"," ");

            // Build human summary per event type
            QString summary;
            if (event == "ReadingSubmitted")
                summary = QString("Sensor %1  →  %2 °C  (round %3)")
                              .arg(e["sensor"].toString().left(10) + "…")
                              .arg(e["valueScaled"].toString())
                              .arg(e["roundId"].toString());
            else if (event == "FaultySensorDetected")
                summary = QString("FAULTY %1  score=%2  threshold=%3")
                              .arg(e["sensor"].toString().left(10) + "…")
                              .arg(e["disagreementScore"].toString())
                              .arg(e["scoreThreshold"].toString());
            else if (event == "ConsensusReached")
                summary = QString("Round %1  →  %2 °C  (%3 trusted, %4 faulty)")
                              .arg(e["roundId"].toString())
                              .arg(e["consensusValueScaled"].toString())
                              .arg(e["trustedParticipants"].toString())
                              .arg(e["faultyCount"].toString());
            else if (event == "ConsensusRejected")
                summary = QString("Round %1 REJECTED  (%2 faulty / %3 total)")
                              .arg(e["roundId"].toString())
                              .arg(e["faultyCount"].toString())
                              .arg(e["totalParticipants"].toString());
            else if (event == "DeviceRegistered")
                summary = QString("Device %1  type=%2  v%3")
                              .arg(e["deviceAddress"].toString().left(10) + "…")
                              .arg(e["deviceType"].toString())
                              .arg(e["firmwareVersion"].toString());
            else if (event == "FirmwareUpdated")
                summary = QString("Device %1  →  v%2")
                              .arg(e["deviceAddress"].toString().left(10) + "…")
                              .arg(e["newVersion"].toString());
            else
                summary = e["deviceAddress"].toString().left(20);

            appendEventRow(event, summary, txHash, block, ts);
        }
    };

    addGroup("readingsSubmitted");
    addGroup("faultySensors");
    addGroup("consensusReached");
    addGroup("consensusRejected");
    addGroup("newRounds");
    addGroup("devicesRegistered");
    addGroup("devicesDeactivated");
    addGroup("devicesReactivated");
    addGroup("firmwareUpdated");

    m_eventsTable->sortByColumn(3, Qt::DescendingOrder);
}

void BlockchainExplorerPage::onError(const QString &endpoint, const QString &msg)
{
    // Restore button states on error
    m_fetchBlockBtn->setEnabled(true);
    m_fetchBlockBtn->setText("Fetch Block");
    m_fetchTxBtn->setEnabled(true);
    m_fetchTxBtn->setText("Fetch Tx");
    m_refreshEventsBtn->setEnabled(true);
    m_refreshEventsBtn->setText("↻ Refresh");

    if (endpoint.contains("/blockchain/block")) {
        m_blockDetail->setPlainText("Error: " + msg);
    } else if (endpoint.contains("/blockchain/tx")) {
        m_txDetail->setPlainText("Error: " + msg);
    }
    // Other errors are handled by the global error signal consumer
}

// ============================================================
// Helpers
// ============================================================

void BlockchainExplorerPage::showJson(QTextEdit *te, const QJsonObject &obj)
{
    QJsonDocument doc(obj);
    te->setPlainText(doc.toJson(QJsonDocument::Indented));
}

void BlockchainExplorerPage::appendEventRow(const QString &event,
                                             const QString &summary,
                                             const QString &txHash,
                                             int blockNumber,
                                             const QString &timestamp)
{
    int row = m_eventsTable->rowCount();
    m_eventsTable->insertRow(row);

    auto *eventItem = new QTableWidgetItem(event);
    // Colour-code by event type
    if (event.contains("Faulty") || event.contains("Rejected"))
        eventItem->setForeground(QColor("#ff6b6b"));
    else if (event.contains("Reached") || event.contains("Registered"))
        eventItem->setForeground(QColor("#51cf66"));
    else if (event.contains("Deactivated"))
        eventItem->setForeground(QColor("#ffd43b"));
    else
        eventItem->setForeground(QColor("#7c83fd"));

    m_eventsTable->setItem(row, 0, eventItem);
    m_eventsTable->setItem(row, 1, new QTableWidgetItem(summary));
    m_eventsTable->setItem(row, 2, new QTableWidgetItem(txHash.left(18) + "…"));
    m_eventsTable->setItem(row, 3, new QTableWidgetItem(QString::number(blockNumber)));
    m_eventsTable->setItem(row, 4, new QTableWidgetItem(timestamp));
}
