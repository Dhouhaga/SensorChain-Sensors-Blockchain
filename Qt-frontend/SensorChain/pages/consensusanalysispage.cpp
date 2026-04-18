// consensusanalysispage.cpp
#include "consensusanalysispage.h"
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>

// ============================================================
// Constructor
// ============================================================

ConsensusAnalysisPage::ConsensusAnalysisPage(ApiClient *api, QWidget *parent)
    : QWidget(parent)
    , m_api(api)
{
    setupUi();
    setupConnections();
}

void ConsensusAnalysisPage::refresh()
{
    // Nothing to auto-refresh — user selects the round manually.
}

// ============================================================
// UI setup
// ============================================================

void ConsensusAnalysisPage::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    // ── Header ────────────────────────────────────────────────
    auto *titleLabel = new QLabel("Consensus Analysis");
    titleLabel->setObjectName("pageTitle");
    auto *sub = new QLabel("Step-by-step breakdown of the pairwise fault-detection algorithm");
    sub->setObjectName("pageSubtitle");
    root->addWidget(titleLabel);
    root->addWidget(sub);

    // ── Round selector ────────────────────────────────────────
    auto *selBox    = new QGroupBox("Select Round");
    auto *selLayout = new QHBoxLayout(selBox);
    selLayout->addWidget(new QLabel("Round ID:"));
    m_roundSpin = new QSpinBox();
    m_roundSpin->setMinimum(1);
    m_roundSpin->setMaximum(9999);
    m_roundSpin->setValue(1);
    selLayout->addWidget(m_roundSpin);
    m_loadBtn = new QPushButton("Load & Explain");
    m_loadBtn->setObjectName("primaryBtn");
    selLayout->addWidget(m_loadBtn);
    selLayout->addStretch();
    root->addWidget(selBox);

    // ── Main splitter ─────────────────────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(2);

    // Left: sensor table
    auto *leftWidget = new QWidget();
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 8, 0);
    auto *tableTitle = new QLabel("Sensor Readings");
    tableTitle->setObjectName("pageSubtitle");
    leftLayout->addWidget(tableTitle);

    m_sensorTable = new QTableWidget(0, 4);
    m_sensorTable->setHorizontalHeaderLabels({"Address", "Value (°C)", "Disagree Score", "Status"});
    m_sensorTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_sensorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sensorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sensorTable->verticalHeader()->setVisible(false);
    leftLayout->addWidget(m_sensorTable, 1);
    splitter->addWidget(leftWidget);

    // Right: algorithm narrative
    auto *rightWidget = new QWidget();
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    auto *algTitle = new QLabel("Algorithm Walkthrough");
    algTitle->setObjectName("pageSubtitle");
    rightLayout->addWidget(algTitle);
    m_algorithmText = new QTextEdit();
    m_algorithmText->setReadOnly(true);
    m_algorithmText->setPlaceholderText("Select a round and click \"Load & Explain\" to see the step-by-step algorithm.");
    // Override text colour to dark for readability on white background
    m_algorithmText->setStyleSheet("QTextEdit { color: #212529; background-color: #f8f9fa; }");
    rightLayout->addWidget(m_algorithmText, 1);
    splitter->addWidget(rightWidget);

    splitter->setSizes({420, 580});
    root->addWidget(splitter, 1);

    // ── Result bar ────────────────────────────────────────────
    auto *resultBox    = new QGroupBox("Round Result");
    auto *resultLayout = new QHBoxLayout(resultBox);

    auto makeResultCard = [&](const QString &label, QLabel *&valRef) {
        auto *vl = new QVBoxLayout();
        auto *lbl = new QLabel(label);
        lbl->setObjectName("pageSubtitle");
        valRef = new QLabel("–");
        valRef->setObjectName("statValueSmall");
        vl->addWidget(lbl);
        vl->addWidget(valRef);
        resultLayout->addLayout(vl);
        resultLayout->addSpacing(24);
    };

    makeResultCard("Consensus Value",   m_resultValue);
    makeResultCard("Trusted Sensors",   m_resultTrusted);
    makeResultCard("Faulty Sensors",    m_resultFaulty);
    makeResultCard("Status",            m_resultStatus);

    resultLayout->addStretch();

    // Tx info (smaller, right-aligned)
    auto *traceVl = new QVBoxLayout();
    auto *traceTitle = new QLabel("Blockchain Trace");
    traceTitle->setObjectName("pageSubtitle");
    m_resultTxHash   = new QLabel("Tx: –");
    m_resultBlock    = new QLabel("Block: –");
    m_resultTimestamp = new QLabel("–");
    m_resultTxHash->setStyleSheet("color: #4c6ef5; font-size: 11px;");
    m_resultBlock->setStyleSheet("color: #4c6ef5; font-size: 11px;");
    m_resultTimestamp->setStyleSheet("color: #868e96; font-size: 10px;");
    traceVl->addWidget(traceTitle);
    traceVl->addWidget(m_resultTxHash);
    traceVl->addWidget(m_resultBlock);
    traceVl->addWidget(m_resultTimestamp);
    resultLayout->addLayout(traceVl);

    root->addWidget(resultBox);
}

// ============================================================
// Connections
// ============================================================

void ConsensusAnalysisPage::setupConnections()
{
    connect(m_loadBtn, &QPushButton::clicked,
            this,      &ConsensusAnalysisPage::onLoadRound);
    connect(m_api, &ApiClient::consensusExplainReady,
            this,  &ConsensusAnalysisPage::onExplainReady);
    connect(m_api, &ApiClient::requestError,
            this,  &ConsensusAnalysisPage::onError);
}

// ============================================================
// Slots
// ============================================================

void ConsensusAnalysisPage::onLoadRound()
{
    m_loadBtn->setEnabled(false);
    m_loadBtn->setText("Loading…");
    m_algorithmText->clear();
    m_sensorTable->setRowCount(0);
    m_api->getConsensusExplain(m_roundSpin->value());
}

void ConsensusAnalysisPage::onExplainReady(QJsonObject data)
{
    m_loadBtn->setEnabled(true);
    m_loadBtn->setText("Load & Explain");

    populateSensorTable(data["step2_faultDetection"].toObject());
    populateAlgorithmText(data);
    populateResultBar(data["result"].toObject(),
                      data["blockchainTrace"].toObject());
}

void ConsensusAnalysisPage::onError(const QString &endpoint, const QString &msg)
{
    if (!endpoint.contains("/explain")) return;
    m_loadBtn->setEnabled(true);
    m_loadBtn->setText("Load & Explain");
    m_algorithmText->setPlainText("Error loading explanation:\n" + msg);
}

// ============================================================
// Populate helpers
// ============================================================

void ConsensusAnalysisPage::populateSensorTable(const QJsonObject &step2)
{
    m_sensorTable->setRowCount(0);
    QJsonArray sensors = step2["sensors"].toArray();

    for (const auto &sv : sensors) {
        QJsonObject s   = sv.toObject();
        int row         = m_sensorTable->rowCount();
        m_sensorTable->insertRow(row);

        QString addr  = s["address"].toString();
        QString val   = s["submittedValue"].toString();
        QString score = s["disagreementScore"].toString()
                        + " (" + s["disagreementScoreScaled"].toString() + ")";
        bool    faulty = s["isFaulty"].toBool();
        QString status = faulty ? "FAULTY" : "TRUSTED";

        m_sensorTable->setItem(row, 0, new QTableWidgetItem(addr.left(12) + "…"));
        m_sensorTable->setItem(row, 1, new QTableWidgetItem(val));
        m_sensorTable->setItem(row, 2, new QTableWidgetItem(score));

        auto *statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(faulty ? QColor("#c92a2a") : QColor("#2b8c4e"));
        statusItem->setFont([faulty]{ QFont f; f.setBold(true); return f; }());
        m_sensorTable->setItem(row, 3, statusItem);

        // Row background tint
        QColor bg = faulty ? QColor(255, 245, 245) : QColor(235, 251, 238);
        for (int c = 0; c < 4; c++) {
            if (m_sensorTable->item(row, c))
                m_sensorTable->item(row, c)->setBackground(bg);
        }
    }
}

void ConsensusAnalysisPage::populateAlgorithmText(const QJsonObject &data)
{
    QString out;

    // ── Inputs ────────────────────────────────────────────────
    out += "════════════════════════════════════════\n";
    out += "STEP 1 — SENSOR INPUTS\n";
    out += "════════════════════════════════════════\n";
    QJsonObject inputs = data["inputs"].toObject();
    out += QString("  %1 sensor(s) submitted readings this round.\n\n")
               .arg(inputs["sensorCount"].toInt());
    for (const auto &sv : inputs["sensors"].toArray()) {
        QJsonObject s = sv.toObject();
        out += QString("  [%1…]  →  %2 °C  (raw: %3)\n")
                   .arg(s["address"].toString().left(12))
                   .arg(s["valueScaled"].toString())
                   .arg(s["valueRaw"].toString());
    }

    // ── Pairwise comparisons ──────────────────────────────────
    out += "\n════════════════════════════════════════\n";
    out += "STEP 2 — PAIRWISE DISAGREEMENT SCORING\n";
    out += "════════════════════════════════════════\n";
    QJsonObject step1 = data["step1_pairwiseComparisons"].toObject();
    out += "  For every unique pair (i,j):\n";
    out += "    diff = |value_i − value_j|\n";
    out += "    scores[i] += diff,  scores[j] += diff\n\n";
    for (const auto &pv : step1["pairs"].toArray()) {
        QJsonObject p = pv.toObject();
        out += QString("  %1… (%2°C)  ↔  %3… (%4°C)  →  diff = %5°C\n")
                   .arg(p["sensorA"].toString().left(10))
                   .arg(p["sensorAValue"].toString())
                   .arg(p["sensorB"].toString().left(10))
                   .arg(p["sensorBValue"].toString())
                   .arg(p["differenceScaled"].toString());
    }

    // ── Fault detection ───────────────────────────────────────
    out += "\n════════════════════════════════════════\n";
    out += "STEP 3 — FAULT DETECTION\n";
    out += "════════════════════════════════════════\n";
    QJsonObject step2 = data["step2_faultDetection"].toObject();
    out += QString("  Threshold formula: %1\n\n").arg(step2["formula"].toString());
    for (const auto &sv : step2["sensors"].toArray()) {
        QJsonObject s = sv.toObject();
        out += QString("  %1…\n    %2\n\n")
                   .arg(s["address"].toString().left(14))
                   .arg(s["verdict"].toString());
    }

    // ── Safety check ──────────────────────────────────────────
    out += "════════════════════════════════════════\n";
    out += "STEP 4 — MAJORITY SAFETY CHECK\n";
    out += "════════════════════════════════════════\n";
    QJsonObject step3 = data["step3_safetyCheck"].toObject();
    out += QString("  %1\n").arg(step3["verdict"].toString());
    out += QString("  Total: %1  |  Faulty: %2  |  Limit: %3\n")
               .arg(step3["totalSensors"].toInt())
               .arg(step3["faultySensors"].toInt())
               .arg(step3["majorityLimit"].toInt());

    // ── Trusted average ───────────────────────────────────────
    out += "\n════════════════════════════════════════\n";
    out += "STEP 5 — TRUSTED AVERAGE\n";
    out += "════════════════════════════════════════\n";
    QJsonObject step4 = data["step4_trustedAverage"].toObject();
    out += QString("  Trusted sensors: %1\n").arg(step4["trustedCount"].toInt());
    for (const auto &tv : step4["trustedSensors"].toArray()) {
        QJsonObject t = tv.toObject();
        out += QString("    %1…  =  %2°C\n")
                   .arg(t["address"].toString().left(12))
                   .arg(t["valueScaled"].toString());
    }
    out += QString("\n  Sum = %1  →  Average = %2°C\n")
               .arg(step4["trustedSumRaw"].toString())
               .arg(step4["computedAverageScaled"].toString());
    out += QString("  Stored consensus value: %1°C\n")
               .arg(step4["storedConsensusScaled"].toString());

    m_algorithmText->setPlainText(out);
}

void ConsensusAnalysisPage::populateResultBar(const QJsonObject &result,
                                              const QJsonObject &trace)
{
    bool reached = result["consensusReached"].toBool();

    m_resultValue->setText(
        reached ? (result["consensusValueScaled"].toString() + " °C") : "N/A"
        );
    m_resultTrusted->setText(result["trustedParticipants"].toString());
    m_resultFaulty->setText(result["faultyCount"].toString());

    m_resultStatus->setText(reached ? "REACHED" : "REJECTED");
    m_resultStatus->setStyleSheet(
        QString("color: %1; font-weight: bold; font-size: 14px;")
            .arg(reached ? "#2b8c4e" : "#c92a2a")
        );

    // Blockchain trace
    if (!trace.isEmpty() && trace["txHash"].isString()) {
        QString txHash  = trace["txHash"].toString();
        int     block   = trace["blockNumber"].toInt();
        QString ts      = trace["blockTimestampISO"].toString().left(19).replace("T", " ");
        m_resultTxHash->setText("Tx: " + txHash.left(20) + "…");
        m_resultBlock->setText("Block: " + QString::number(block));
        m_resultTimestamp->setText(ts);
    } else {
        m_resultTxHash->setText("Tx: –");
        m_resultBlock->setText("Block: –");
        m_resultTimestamp->setText("–");
    }
}
