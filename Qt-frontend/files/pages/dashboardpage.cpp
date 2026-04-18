#include "dashboardpage.h"
#include <QDateTime>
#include <QHeaderView>

DashboardPage::DashboardPage(ApiClient *api, QWidget *parent)
    : QWidget(parent), m_api(api)
{
    setupUi();

    // Connect API signals
    connect(m_api, &ApiClient::latestConsensusReady, this, &DashboardPage::onLatestConsensus);
    connect(m_api, &ApiClient::consensusStatsReady,  this, &DashboardPage::onConsensusStats);
    connect(m_api, &ApiClient::currentRoundReady,    this, &DashboardPage::onCurrentRound);
    connect(m_api, &ApiClient::registryStatsReady,   this, &DashboardPage::onRegistryStats);
    connect(m_api, &ApiClient::requestError,         this, &DashboardPage::onError);

    // Auto-refresh every 5 seconds
    m_autoRefresh = new QTimer(this);
    connect(m_autoRefresh, &QTimer::timeout, this, &DashboardPage::refresh);
    m_autoRefresh->start(5000);

    refresh();
}

void DashboardPage::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // ── Page header ──────────────────────────────────────────────────
    QLabel *title = new QLabel("Dashboard");
    title->setObjectName("pageTitle");

    QLabel *subtitle = new QLabel("Live consensus monitoring and network status");
    subtitle->setObjectName("pageSubtitle");

    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    // ── Top row: consensus value (big) + status ──────────────────────
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(16);

    // Consensus value card (large)
    QGroupBox *consensusCard = new QGroupBox("Latest Consensus Value");
    consensusCard->setMinimumHeight(140);
    QVBoxLayout *consensusLayout = new QVBoxLayout(consensusCard);
    consensusLayout->setAlignment(Qt::AlignCenter);

    m_consensusBig = new QLabel("--");
    m_consensusBig->setObjectName("consensusBig");
    m_consensusBig->setAlignment(Qt::AlignCenter);

    m_consensusTempLabel = new QLabel("°C  (scaled ×100)");
    m_consensusTempLabel->setAlignment(Qt::AlignCenter);
    m_consensusTempLabel->setObjectName("pageSubtitle");

    consensusLayout->addWidget(m_consensusBig);
    consensusLayout->addWidget(m_consensusTempLabel);
    topRow->addWidget(consensusCard, 2);

    // Status cards grid
    QWidget *statsWidget = new QWidget();
    QGridLayout *statsGrid = new QGridLayout(statsWidget);
    statsGrid->setSpacing(12);

    statsGrid->addWidget(makeStatCard("Current Round",    m_currentRoundLabel,  "#7c83fd"), 0, 0);
    statsGrid->addWidget(makeStatCard("Total Rounds",     m_totalRoundsLabel,   "#51cf66"), 0, 1);
    statsGrid->addWidget(makeStatCard("Total Devices",    m_totalDevicesLabel,  "#ffd43b"), 1, 0);
    statsGrid->addWidget(makeStatCard("Trusted Sensors",  m_trustedCountLabel,  "#51cf66"), 1, 1);

    topRow->addWidget(statsWidget, 3);
    mainLayout->addLayout(topRow);

    // ── Second row: round info + faulty + settings ───────────────────
    QHBoxLayout *midRow = new QHBoxLayout();
    midRow->setSpacing(16);

    // Round progress
    QGroupBox *roundGroup = new QGroupBox("Current Round Status");
    QVBoxLayout *roundLayout = new QVBoxLayout(roundGroup);

    QHBoxLayout *roundInfoRow = new QHBoxLayout();
    QLabel *roundIdLbl = new QLabel("Round ID:");
    roundIdLbl->setObjectName("pageSubtitle");
    m_currentRoundLabel2 = new QLabel("--");
    m_currentRoundLabel2->setObjectName("statValueSmall");

    QLabel *participantsLbl = new QLabel("Participants:");
    participantsLbl->setObjectName("pageSubtitle");
    m_participantsCountLabel = new QLabel("--");
    m_participantsCountLabel->setObjectName("statValueSmall");

    QLabel *minLbl = new QLabel("Required:");
    minLbl->setObjectName("pageSubtitle");
    m_minSensorsLabel = new QLabel("--");
    m_minSensorsLabel->setObjectName("statValueSmall");

    roundInfoRow->addWidget(roundIdLbl);
    roundInfoRow->addWidget(m_currentRoundLabel2);
    roundInfoRow->addSpacing(16);
    roundInfoRow->addWidget(participantsLbl);
    roundInfoRow->addWidget(m_participantsCountLabel);
    roundInfoRow->addSpacing(16);
    roundInfoRow->addWidget(minLbl);
    roundInfoRow->addWidget(m_minSensorsLabel);
    roundInfoRow->addStretch();
    roundLayout->addLayout(roundInfoRow);

    QLabel *timeLbl = new QLabel("Time remaining:");
    timeLbl->setObjectName("pageSubtitle");
    m_timeRemainingLabel = new QLabel("--");
    roundLayout->addWidget(timeLbl);
    roundLayout->addWidget(m_timeRemainingLabel);

    m_roundProgress = new QProgressBar();
    m_roundProgress->setRange(0, 100);
    m_roundProgress->setValue(0);
    m_roundProgress->setTextVisible(true);
    m_roundProgress->setFixedHeight(20);
    roundLayout->addWidget(m_roundProgress);

    midRow->addWidget(roundGroup, 2);

    // Faulty + threshold info
    QGroupBox *faultyGroup = new QGroupBox("Fault Detection");
    QFormLayout *faultyLayout = new QFormLayout(faultyGroup);

    m_faultyCountLabel = new QLabel("--");
    m_faultyCountLabel->setObjectName("statValueSmall");

    m_thresholdLabel = new QLabel("--");
    m_statusLabel    = new QLabel("--");
    m_lastUpdatedLabel = new QLabel("--");

    faultyLayout->addRow("Faulty sensors (last round):", m_faultyCountLabel);
    faultyLayout->addRow("Fault threshold (×100):",      m_thresholdLabel);
    faultyLayout->addRow("Network status:",               m_statusLabel);
    faultyLayout->addRow("Last updated:",                 m_lastUpdatedLabel);

    midRow->addWidget(faultyGroup, 1);
    mainLayout->addLayout(midRow);

    // ── Bottom row: current participants table ───────────────────────
    QGroupBox *participantsGroup = new QGroupBox("Current Round Participants");
    QVBoxLayout *participantsLayout = new QVBoxLayout(participantsGroup);

    m_participantsTable = new QTableWidget(0, 2);
    m_participantsTable->setHorizontalHeaderLabels({"Sensor Address", "Status"});
    m_participantsTable->horizontalHeader()->setStretchLastSection(true);
    m_participantsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_participantsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_participantsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_participantsTable->setMaximumHeight(160);
    participantsLayout->addWidget(m_participantsTable);

    mainLayout->addWidget(participantsGroup);
    mainLayout->addStretch();
}

QGroupBox *DashboardPage::makeStatCard(const QString &title, QLabel *&valueLabel,
                                        const QString &color)
{
    QGroupBox *card = new QGroupBox(title);
    card->setMinimumHeight(80);
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setAlignment(Qt::AlignCenter);

    valueLabel = new QLabel("--");
    valueLabel->setObjectName("statValueSmall");
    valueLabel->setAlignment(Qt::AlignCenter);
    valueLabel->setStyleSheet("color: " + color + "; font-size: 22px; font-weight: bold;");
    layout->addWidget(valueLabel);

    return card;
}

void DashboardPage::refresh()
{
    m_api->getLatestConsensus();
    m_api->getConsensusStats();
    m_api->getCurrentRound();
    m_api->getRegistryStats();
}

void DashboardPage::onLatestConsensus(QJsonObject data)
{
    QString val     = data["consensusValue"].toString("--");
    QString scaled  = data["consensusValueScaled"].toString("--");
    QString trusted = data["trustedCount"].toString("--");
    QString faulty  = data["faultyCount"].toString("--");
    QString ts      = data["timestamp"].toString("0");

    m_consensusBig->setText(val);
    m_consensusTempLabel->setText(scaled + " °C  (raw value ÷ 100)");
    m_trustedCountLabel->setText(trusted);

    // Color faulty count
    int faultyInt = faulty.toInt();
    m_faultyCountLabel->setText(faulty);
    m_faultyCountLabel->setStyleSheet(faultyInt > 0
        ? "color: #ff6b6b; font-size: 18px; font-weight: bold;"
        : "color: #51cf66; font-size: 18px; font-weight: bold;");

    // Status
    if (faultyInt == 0) {
        m_statusLabel->setText("✅  All sensors healthy");
        m_statusLabel->setObjectName("statusOk");
    } else {
        m_statusLabel->setText(QString("⚠️  %1 faulty sensor(s) detected").arg(faultyInt));
        m_statusLabel->setObjectName("statusWarning");
    }

    // Last updated
    if (ts != "0") {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(ts.toLongLong());
        m_lastUpdatedLabel->setText(dt.toString("hh:mm:ss  dd/MM/yyyy"));
    }
}

void DashboardPage::onConsensusStats(QJsonObject data)
{
    m_currentRoundLabel->setText(data["currentRoundId"].toString("--"));
    m_currentRoundLabel2->setText(data["currentRoundId"].toString("--"));
    m_totalRoundsLabel->setText(data["totalRounds"].toString("--"));
    m_minSensorsLabel->setText(data["minSensorsForConsensus"].toString("--"));
    m_thresholdLabel->setText(data["faultyThresholdUnits"].toString("--")
                              + "  (" + data["faultyThresholdScaled"].toString("--") + "°)");

    // Round time remaining
    int remaining = data["roundTimeRemainingSeconds"].toInt(0);
    int window    = data["consensusWindow"].toString("3600").toInt();
    m_timeRemaining = remaining;
    m_consensusWindow = window;

    int seconds  = remaining % 60;
    int minutes  = (remaining / 60) % 60;
    int hours    = remaining / 3600;
    m_timeRemainingLabel->setText(
        QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds));

    int pct = (window > 0) ? (int)(((double)remaining / window) * 100.0) : 0;
    m_roundProgress->setValue(pct);
    m_roundProgress->setFormat(QString("%1%  remaining").arg(pct));
}

void DashboardPage::onCurrentRound(QJsonObject data)
{
    QJsonArray participants = data["participants"].toArray();
    int count    = participants.size();
    int minReq   = data["minSensorsRequired"].toString("3").toInt();

    m_participantsCountLabel->setText(QString("%1 / %2").arg(count).arg(minReq));

    m_participantsTable->setRowCount(0);
    for (int i = 0; i < participants.size(); i++) {
        QString addr = participants[i].toString();
        m_participantsTable->insertRow(i);

        QTableWidgetItem *addrItem = new QTableWidgetItem(addr);
        QTableWidgetItem *statusItem = new QTableWidgetItem("✅  Submitted");
        statusItem->setForeground(QColor("#51cf66"));

        m_participantsTable->setItem(i, 0, addrItem);
        m_participantsTable->setItem(i, 1, statusItem);
    }
}

void DashboardPage::onRegistryStats(QJsonObject data)
{
    m_totalDevicesLabel->setText(data["totalDevices"].toString("--"));
}

void DashboardPage::onError(QString endpoint, QString error)
{
    Q_UNUSED(endpoint)
    Q_UNUSED(error)
    // Dashboard silently ignores errors — shows -- for missing data
}
