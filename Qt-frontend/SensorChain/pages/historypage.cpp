// historypage.cpp
#include "historypage.h"
#include <QHeaderView>
#include <QDateTime>

HistoryPage::HistoryPage(ApiClient *api, QWidget *parent)
    : QWidget(parent), m_api(api)
{
    setupUi();

    connect(m_api, &ApiClient::allRoundsReady,      this, &HistoryPage::onAllRounds);
    connect(m_api, &ApiClient::consensusRoundReady, this, &HistoryPage::onConsensusRound);
    connect(m_api, &ApiClient::eventHistoryReady,   this, &HistoryPage::onEventHistory);
    connect(m_api, &ApiClient::requestError,        this, &HistoryPage::onError);

    refresh();
}

void HistoryPage::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel *title = new QLabel("History");
    title->setObjectName("pageTitle");
    QLabel *subtitle = new QLabel("All consensus rounds and blockchain event log");
    subtitle->setObjectName("pageSubtitle");
    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    QTabWidget *tabs = new QTabWidget();
    tabs->addTab(buildRoundsTab(), "Consensus Rounds");
    tabs->addTab(buildEventsTab(), "Event Log");
    mainLayout->addWidget(tabs, 1);
}

QWidget *HistoryPage::buildRoundsTab()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setSpacing(12);

    // Top bar: refresh + lookup
    QHBoxLayout *topBar = new QHBoxLayout();
    m_refreshBtn = new QPushButton("Refresh All Rounds");
    m_refreshBtn->setObjectName("primaryBtn");

    QLabel *lookupLbl = new QLabel("Jump to Round:");
    m_roundIdInput = new QSpinBox();
    m_roundIdInput->setRange(1, 99999);
    m_roundIdInput->setValue(1);
    m_lookupBtn = new QPushButton("Load");

    topBar->addWidget(m_refreshBtn);
    topBar->addStretch();
    topBar->addWidget(lookupLbl);
    topBar->addWidget(m_roundIdInput);
    topBar->addWidget(m_lookupBtn);
    layout->addLayout(topBar);

    // Rounds table
    m_roundsTable = new QTableWidget(0, 7);
    m_roundsTable->setHorizontalHeaderLabels({
        "Round", "Consensus Value", "°C", "Trusted", "Faulty", "Status", "Timestamp"
    });
    m_roundsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_roundsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_roundsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_roundsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_roundsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_roundsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_roundsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_roundsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_roundsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_roundsTable->setMaximumHeight(220);
    layout->addWidget(m_roundsTable);

    // Per-sensor breakdown
    m_roundDetailTitle = new QLabel("Click a round to see per-sensor breakdown");
    m_roundDetailTitle->setObjectName("pageSubtitle");
    layout->addWidget(m_roundDetailTitle);

    m_roundSummaryLabel = new QLabel("");
    layout->addWidget(m_roundSummaryLabel);

    m_sensorBreakdownTable = new QTableWidget(0, 5);
    m_sensorBreakdownTable->setHorizontalHeaderLabels({
        "Sensor Address", "Value (×100)", "°C", "Disagreement Score", "Status"
    });
    m_sensorBreakdownTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_sensorBreakdownTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_sensorBreakdownTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_sensorBreakdownTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_sensorBreakdownTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_sensorBreakdownTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_sensorBreakdownTable, 1);

    connect(m_refreshBtn,    &QPushButton::clicked, this, &HistoryPage::refresh);
    connect(m_lookupBtn,     &QPushButton::clicked, this, &HistoryPage::onLookupRoundClicked);
    connect(m_roundsTable,   &QTableWidget::cellClicked, this, &HistoryPage::onRoundRowClicked);

    return container;
}

QWidget *HistoryPage::buildEventsTab()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setSpacing(12);

    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *fromBlockLbl = new QLabel("From Block:");
    m_fromBlockInput = new QSpinBox();
    m_fromBlockInput->setRange(0, 9999999);
    m_fromBlockInput->setValue(0);
    m_loadEventsBtn = new QPushButton("Load Events");
    m_loadEventsBtn->setObjectName("primaryBtn");
    topBar->addWidget(fromBlockLbl);
    topBar->addWidget(m_fromBlockInput);
    topBar->addStretch();
    topBar->addWidget(m_loadEventsBtn);
    layout->addLayout(topBar);

    m_eventTree = new QTreeWidget();
    m_eventTree->setHeaderLabels({"Event", "Details"});
    m_eventTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_eventTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_eventTree->setAlternatingRowColors(true);
    layout->addWidget(m_eventTree, 1);

    connect(m_loadEventsBtn, &QPushButton::clicked, [this]() {
        m_api->getEventHistory(m_fromBlockInput->value());
    });

    return container;
}

void HistoryPage::refresh()
{
    m_api->getAllRounds();
}

void HistoryPage::onAllRounds(QJsonArray rounds)
{
    populateRoundsTable(rounds);
}

void HistoryPage::populateRoundsTable(const QJsonArray &rounds)
{
    m_roundsTable->setRowCount(0);
    for (int i = 0; i < rounds.size(); i++) {
        QJsonObject r = rounds[i].toObject();
        m_roundsTable->insertRow(i);

        bool reached = r["consensusReached"].toBool();
        QString status = reached ? "Reached" : "Rejected";

        m_roundsTable->setItem(i, 0, new QTableWidgetItem(r["roundId"].toString()));
        m_roundsTable->setItem(i, 1, new QTableWidgetItem(r["consensusValue"].toString()));
        m_roundsTable->setItem(i, 2, new QTableWidgetItem(r["consensusValueScaled"].toString() + " °C"));
        m_roundsTable->setItem(i, 3, new QTableWidgetItem(r["trustedParticipants"].toString()));

        int faultyCount = r["faultyCount"].toString("0").toInt();
        QTableWidgetItem *faultyItem = new QTableWidgetItem(r["faultyCount"].toString());
        if (faultyCount > 0) faultyItem->setForeground(QColor("#c92a2a"));
        else faultyItem->setForeground(QColor("#2b8c4e"));
        m_roundsTable->setItem(i, 4, faultyItem);

        QTableWidgetItem *statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(reached ? QColor("#2b8c4e") : QColor("#c92a2a"));
        m_roundsTable->setItem(i, 5, statusItem);

        QString ts = r["timestamp"].toString("0");
        QDateTime dt = QDateTime::fromSecsSinceEpoch(ts.toLongLong());
        m_roundsTable->setItem(i, 6, new QTableWidgetItem(dt.toString("dd/MM/yyyy  hh:mm:ss")));
    }
}

void HistoryPage::onRoundRowClicked(int row, int col)
{
    Q_UNUSED(col)
    int roundId = m_roundsTable->item(row, 0)->text().toInt();
    m_api->getConsensusRound(roundId);
}

void HistoryPage::onLookupRoundClicked()
{
    m_api->getConsensusRound(m_roundIdInput->value());
}

void HistoryPage::onConsensusRound(QJsonObject round)
{
    populateRoundDetail(round);
}

void HistoryPage::populateRoundDetail(const QJsonObject &round)
{
    QString roundId = round["roundId"].toString();
    bool reached    = round["consensusReached"].toBool();

    m_roundDetailTitle->setText(
        QString("Round %1 — %2  |  Consensus: %3 (%4 °C)  |  Trusted: %5  |  Faulty: %6")
            .arg(roundId)
            .arg(reached ? "Reached" : "Rejected")
            .arg(round["consensusValue"].toString())
            .arg(round["consensusValueScaled"].toString())
            .arg(round["trustedParticipants"].toString())
            .arg(round["faultyCount"].toString())
        );

    // Per-sensor breakdown
    QJsonArray sensors = round["sensors"].toArray();
    m_sensorBreakdownTable->setRowCount(0);

    for (int i = 0; i < sensors.size(); i++) {
        QJsonObject s = sensors[i].toObject();
        m_sensorBreakdownTable->insertRow(i);

        bool isFaulty = s["isFaulty"].toBool();

        m_sensorBreakdownTable->setItem(i, 0, new QTableWidgetItem(s["address"].toString()));
        m_sensorBreakdownTable->setItem(i, 1, new QTableWidgetItem(s["value"].toString()));
        m_sensorBreakdownTable->setItem(i, 2, new QTableWidgetItem(s["valueScaled"].toString() + " °C"));
        m_sensorBreakdownTable->setItem(i, 3, new QTableWidgetItem(s["disagreementScore"].toString()));

        QTableWidgetItem *statusItem = new QTableWidgetItem(isFaulty ? "FAULTY" : "Trusted");
        statusItem->setForeground(isFaulty ? QColor("#c92a2a") : QColor("#2b8c4e"));
        m_sensorBreakdownTable->setItem(i, 4, statusItem);

        // Highlight entire row red if faulty
        if (isFaulty) {
            for (int c = 0; c < 5; c++) {
                if (m_sensorBreakdownTable->item(i, c))
                    m_sensorBreakdownTable->item(i, c)->setBackground(QColor(255, 245, 245));
            }
        }
    }
}

void HistoryPage::onEventHistory(QJsonObject events)
{
    populateEventTree(events);
}

void HistoryPage::populateEventTree(const QJsonObject &events)
{
    m_eventTree->clear();

    auto addCategory = [this](const QString &name, const QJsonArray &arr,
                              std::function<QString(QJsonObject)> formatter,
                              const QColor &color) {
        if (arr.isEmpty()) return;
        QTreeWidgetItem *cat = new QTreeWidgetItem(m_eventTree);
        cat->setText(0, name + "  (" + QString::number(arr.size()) + ")");
        cat->setForeground(0, color);
        cat->setExpanded(true);

        for (int i = 0; i < arr.size(); i++) {
            QJsonObject e = arr[i].toObject();
            QTreeWidgetItem *item = new QTreeWidgetItem(cat);
            item->setText(0, "Block " + QString::number(e["blockNumber"].toInt()));
            item->setText(1, formatter(e));
        }
    };

    addCategory("ConsensusReached", events["consensusReached"].toArray(),
                [](QJsonObject e) {
                    return QString("Round %1  →  %2 (%3 °C)  |  Trusted: %4  Faulty: %5")
                        .arg(e["roundId"].toString())
                        .arg(e["consensusValue"].toString())
                        .arg(e["consensusValueScaled"].toString())
                        .arg(e["trustedParticipants"].toString())
                        .arg(e["faultyCount"].toString());
                }, QColor("#2b8c4e"));

    addCategory("ConsensusRejected", events["consensusRejected"].toArray(),
                [](QJsonObject e) {
                    return QString("Round %1  →  %2 faulty out of %3  |  %4")
                        .arg(e["roundId"].toString())
                        .arg(e["faultyCount"].toString())
                        .arg(e["totalParticipants"].toString())
                        .arg(e["reason"].toString());
                }, QColor("#c92a2a"));

    addCategory("FaultySensorDetected", events["faultySensors"].toArray(),
                [](QJsonObject e) {
                    return QString("Round %1  |  Sensor: %2  |  Value: %3 (%4 °C)  |  Score: %5  >  Threshold: %6")
                        .arg(e["roundId"].toString())
                        .arg(e["sensor"].toString().left(12) + "...")
                        .arg(e["submittedValue"].toString())
                        .arg(e["submittedValueScaled"].toString())
                        .arg(e["disagreementScore"].toString())
                        .arg(e["scoreThreshold"].toString());
                }, QColor("#e67700"));

    addCategory("ReadingSubmitted", events["readingsSubmitted"].toArray(),
                [](QJsonObject e) {
                    return QString("Round %1  |  Sensor: %2  |  Value: %3 (%4 °C)")
                        .arg(e["roundId"].toString())
                        .arg(e["sensor"].toString().left(12) + "...")
                        .arg(e["value"].toString())
                        .arg(e["valueScaled"].toString());
                }, QColor("#4c6ef5"));

    addCategory("NewRoundStarted", events["newRounds"].toArray(),
                [](QJsonObject e) {
                    QDateTime dt = QDateTime::fromSecsSinceEpoch(e["timestamp"].toString("0").toLongLong());
                    return QString("Round %1  started at %2")
                        .arg(e["roundId"].toString())
                        .arg(dt.toString("dd/MM/yyyy hh:mm:ss"));
                }, QColor("#339af0"));

    addCategory("DeviceRegistered", events["devicesRegistered"].toArray(),
                [](QJsonObject e) {
                    return QString("%1  |  Type: %2  |  v%3")
                    .arg(e["deviceAddress"].toString().left(12) + "...")
                        .arg(e["deviceType"].toString())
                        .arg(e["firmwareVersion"].toString());
                }, QColor("#94d82d"));

    addCategory("DeviceDeactivated", events["devicesDeactivated"].toArray(),
                [](QJsonObject e) {
                    return QString("%1").arg(e["deviceAddress"].toString());
                }, QColor("#ff8787"));

    addCategory("DeviceReactivated", events["devicesReactivated"].toArray(),
                [](QJsonObject e) {
                    return QString("%1").arg(e["deviceAddress"].toString());
                }, QColor("#69db7c"));

    addCategory("FirmwareUpdated", events["firmwareUpdated"].toArray(),
                [](QJsonObject e) {
                    return QString("%1  →  v%2")
                        .arg(e["deviceAddress"].toString().left(12) + "...")
                        .arg(e["newVersion"].toString());
                }, QColor("#da77f2"));
}

void HistoryPage::onError(QString endpoint, QString error)
{
    Q_UNUSED(endpoint)
    m_roundDetailTitle->setText("Error: " + error);
}
