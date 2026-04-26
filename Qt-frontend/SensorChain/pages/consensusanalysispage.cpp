#include "consensusanalysispage.h"
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>


ConsensusAnalysisPage::ConsensusAnalysisPage(ApiClient *api, QWidget *parent)
    : QWidget(parent)
    , m_api(api)
{
    setupUi();
    setupConnections();
}

void ConsensusAnalysisPage::refresh()
{
    // Nothing to auto-refresh  user selects the round manually.
}


void ConsensusAnalysisPage::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    //  Header 
    auto *titleLabel = new QLabel("Consensus Analysis");
    titleLabel->setObjectName("pageTitle");
    auto *sub = new QLabel("Step-by-step breakdown of the pairwise fault-detection algorithm");
    sub->setObjectName("pageSubtitle");
    root->addWidget(titleLabel);
    root->addWidget(sub);

    //  Round selector 
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

    //  Main splitter 
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
    m_sensorTable->setHorizontalHeaderLabels({"Address", "Value (C)", "Disagree Score", "Status"});
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
    m_algorithmText->setStyleSheet("QTextEdit { color: #212529; background-color: #f8f9fa; }");
    rightLayout->addWidget(m_algorithmText, 1);
    splitter->addWidget(rightWidget);

    splitter->setSizes({420, 580});
    root->addWidget(splitter, 1);

    //  Result bar 
    auto *resultBox    = new QGroupBox("Round Result");
    auto *resultLayout = new QHBoxLayout(resultBox);

    auto makeResultCard = [&](const QString &label, QLabel *&valRef) {
        auto *vl = new QVBoxLayout();
        auto *lbl = new QLabel(label);
        lbl->setObjectName("pageSubtitle");
        valRef = new QLabel("");
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
    m_resultTxHash   = new QLabel("Tx: ");
    m_resultBlock    = new QLabel("Block: ");
    m_resultTimestamp = new QLabel("");
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


void ConsensusAnalysisPage::setupConnections()
{
    connect(m_loadBtn, &QPushButton::clicked,
            this,      &ConsensusAnalysisPage::onLoadRound);
    connect(m_api, &ApiClient::consensusExplainReady,
            this,  &ConsensusAnalysisPage::onExplainReady);
    connect(m_api, &ApiClient::requestError,
            this,  &ConsensusAnalysisPage::onError);
}


void ConsensusAnalysisPage::onLoadRound()
{
    m_loadBtn->setEnabled(false);
    m_loadBtn->setText("Loading");
    m_algorithmText->clear();
    m_sensorTable->setRowCount(0);
    m_api->getConsensusExplain(m_roundSpin->value());
}

void ConsensusAnalysisPage::onExplainReady(QJsonObject data)
{
    m_loadBtn->setEnabled(true);
    m_loadBtn->setText("Load & Explain");

    qDebug() << "=== FULL CONSENSUS EXPLAIN RESPONSE ===";
    qDebug() << QJsonDocument(data).toJson(QJsonDocument::Indented);
    qDebug() << "======================================";

    qDebug() << "Keys in response:" << data.keys();

    if (data.contains("inputs")) {
        qDebug() << "inputs exists, type:" << data["inputs"].type();
        qDebug() << "inputs:" << data["inputs"].toObject();
    } else {
        qDebug() << "inputs MISSING";
    }

    if (data.contains("step1_pairwiseComparisons")) {
        qDebug() << "step1_pairwiseComparisons exists";
    } else {
        qDebug() << "step1_pairwiseComparisons MISSING";
    }

    if (data.contains("step2_faultDetection")) {
        qDebug() << "step2_faultDetection exists";
        QJsonObject step2 = data["step2_faultDetection"].toObject();
        qDebug() << "step2 keys:" << step2.keys();
        qDebug() << "sensors array:" << step2["sensors"].toArray();
    } else {
        qDebug() << "step2_faultDetection MISSING";
    }

    if (data.contains("step3_safetyCheck")) {
        qDebug() << "step3_safetyCheck exists";
    } else {
        qDebug() << "step3_safetyCheck MISSING";
    }

    if (data.contains("step4_trustedAverage")) {
        qDebug() << "step4_trustedAverage exists";
    } else {
        qDebug() << "step4_trustedAverage MISSING";
    }

    if (data.contains("result")) {
        qDebug() << "result exists";
    } else {
        qDebug() << "result MISSING";
    }

    QJsonObject step2 = data["step2_faultDetection"].toObject();
    populateSensorTable(step2);
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


void ConsensusAnalysisPage::populateSensorTable(const QJsonObject &step2)
{
    qDebug() << "=== populateSensorTable called ===";
    qDebug() << "step2:" << step2;

    m_sensorTable->setRowCount(0);

    if (step2.isEmpty()) {
        qDebug() << "step2 is EMPTY!";
        return;
    }

    QJsonArray sensors = step2["sensors"].toArray();
    qDebug() << "sensors array size:" << sensors.size();

    if (sensors.isEmpty()) {
        qDebug() << "sensors array is EMPTY!";
        return;
    }

    for (const auto &sv : sensors) {
        QJsonObject s = sv.toObject();
        qDebug() << "Processing sensor:" << s;

        int row = m_sensorTable->rowCount();
        m_sensorTable->insertRow(row);

        QString addr = s["address"].toString();
        QString val = s["submittedValue"].toString();
        QString score = s["disagreementScoreScaled"].toString();
        bool faulty = s["isFaulty"].toBool();
        QString status = faulty ? "FAULTY" : "TRUSTED";

        m_sensorTable->setItem(row, 0, new QTableWidgetItem(addr.left(14) + "..."));
        m_sensorTable->setItem(row, 1, new QTableWidgetItem(val + " C"));
        m_sensorTable->setItem(row, 2, new QTableWidgetItem(score));

        auto *statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(faulty ? QColor("#c92a2a") : QColor("#2b8c4e"));
        QFont f;
        f.setBold(true);
        statusItem->setFont(f);
        m_sensorTable->setItem(row, 3, statusItem);
    }

    m_sensorTable->resizeColumnsToContents();
}

void ConsensusAnalysisPage::populateAlgorithmText(const QJsonObject &data)
{
    QString out;

    //  Inputs 
    out += "\n";
    out += "STEP 1  SENSOR INPUTS\n";
    out += "\n";
    QJsonObject inputs = data["inputs"].toObject();
    int sensorCount = inputs["sensorCount"].toInt();
    out += QString("  %1 sensor(s) submitted readings this round.\n\n").arg(sensorCount);

    QJsonArray sensors = inputs["sensors"].toArray();
    for (const auto &sv : sensors) {
        QJsonObject s = sv.toObject();
        QString addr = s["address"].toString();
        QString value = s["valueScaled"].toString();
        out += QString("   %1\n").arg(addr);
        out += QString("    Value: %1 C\n").arg(value);
    }
    out += "\n";

    //  Pairwise comparisons 
    out += "\n";
    out += "STEP 2  PAIRWISE DISAGREEMENT SCORING\n";
    out += "\n";
    out += "  For every unique pair (i, j):\n";
    out += "    diff = |value_i  value_j|\n";
    out += "    scores[i] += diff,  scores[j] += diff\n\n";

    QJsonObject step1 = data["step1_pairwiseComparisons"].toObject();
    QJsonArray pairs = step1["pairs"].toArray();

    if (pairs.size() > 0) {
        out += QString("  Total pairs: %1\n\n").arg(pairs.size());
        // Show first 15 pairs, or fewer if less
        int showCount = qMin(pairs.size(), 15);
        for (int i = 0; i < showCount; i++) {
            QJsonObject p = pairs[i].toObject();
            out += QString("  Pair %1:\n").arg(i+1);
            out += QString("    %1 (%2C)    %3 (%4C)\n")
                       .arg(p["sensorA"].toString().left(16))
                       .arg(p["sensorAValue"].toString())
                       .arg(p["sensorB"].toString().left(16))
                       .arg(p["sensorBValue"].toString());
            out += QString("    Difference: %1C\n\n")
                       .arg(p["differenceScaled"].toString());
        }
        if (pairs.size() > 15) {
            out += QString("  ... and %1 more comparisons\n\n").arg(pairs.size() - 15);
        }
    } else {
        out += "  No pairwise comparisons (less than 2 sensors).\n\n";
    }

    //  Fault detection 
    out += "\n";
    out += "STEP 3  FAULT DETECTION\n";
    out += "\n";
    QJsonObject step2 = data["step2_faultDetection"].toObject();
    out += QString("  Threshold formula: %1\n\n").arg(step2["formula"].toString());
    out += QString("  Threshold value: %1\n\n").arg(step2["scoreThresholdScaled"].toString());

    QJsonArray faultSensors = step2["sensors"].toArray();
    for (const auto &sv : faultSensors) {
        QJsonObject s = sv.toObject();
        out += QString("  Sensor: %1\n").arg(s["address"].toString());
        out += QString("    Submitted: %1C\n").arg(s["submittedValue"].toString());
        out += QString("    Disagreement Score: %1\n").arg(s["disagreementScoreScaled"].toString());
        out += QString("    Threshold: %1\n").arg(s["scoreThresholdScaled"].toString());
        out += QString("    %2\n\n").arg(s["verdict"].toString());
    }

    //  Safety check 
    out += "\n";
    out += "STEP 4  MAJORITY SAFETY CHECK\n";
    out += "\n";
    QJsonObject step3 = data["step3_safetyCheck"].toObject();
    out += QString("  %1\n\n").arg(step3["description"].toString());
    out += QString("  Total sensors: %1\n").arg(step3["totalSensors"].toInt());
    out += QString("  Faulty sensors: %1\n").arg(step3["faultySensors"].toInt());
    out += QString("  Majority limit: %1\n").arg(step3["majorityLimit"].toInt());
    out += QString("  Verdict: %1\n\n").arg(step3["verdict"].toString());

    //  Trusted average (only if round was not rejected) 
    QJsonObject result = data["result"].toObject();
    bool consensusReached = result["consensusReached"].toBool();

    out += "\n";
    out += "STEP 5  TRUSTED AVERAGE CALCULATION\n";
    out += "\n";

    if (consensusReached) {
        QJsonObject step4 = data["step4_trustedAverage"].toObject();
        out += QString("  %1\n\n").arg(step4["description"].toString());

        QJsonArray trustedSensors = step4["trustedSensors"].toArray();
        out += QString("  Trusted sensors (%1):\n").arg(trustedSensors.size());
        for (const auto &tv : trustedSensors) {
            QJsonObject t = tv.toObject();
            out += QString("     %1  %2C\n")
                       .arg(t["address"].toString().left(16))
                       .arg(t["valueScaled"].toString());
        }
        out += QString("\n  Sum of trusted values: %1\n")
                   .arg(step4["trustedSumRaw"].toString());
        out += QString("  Trusted count: %1\n")
                   .arg(step4["trustedCount"].toInt());
        out += QString("  Calculated average: %1C\n")
                   .arg(step4["computedAverageScaled"].toString());
        out += QString("  Stored consensus value: %1C\n")
                   .arg(step4["storedConsensusScaled"].toString());
    } else {
        out += "   Round was REJECTED  trusted average NOT calculated.\n";
        out += "  Reason: Majority of sensors were flagged as faulty.\n";
        out += "  No consensus value was stored for this round.\n";
    }

    m_algorithmText->setPlainText(out);
}

void ConsensusAnalysisPage::populateResultBar(const QJsonObject &result,
                                              const QJsonObject &trace)
{
    bool reached = result["consensusReached"].toBool();

    if (reached) {
        m_resultValue->setText(result["consensusValueScaled"].toString() + " C");
    } else {
        m_resultValue->setText("N/A (Rejected)");
    }

    m_resultTrusted->setText(result["trustedParticipants"].toString());
    m_resultFaulty->setText(result["faultyCount"].toString());

    m_resultStatus->setText(reached ? "CONSENSUS REACHED" : "ROUND REJECTED");
    m_resultStatus->setStyleSheet(
        QString("color: %1; font-weight: bold; font-size: 14px;")
            .arg(reached ? "#2b8c4e" : "#c92a2a")
        );

    if (!trace.isEmpty()) {
        QString txHash = trace["transactionHash"].toString();
        if (txHash.isEmpty()) txHash = trace["txHash"].toString();

        if (!txHash.isEmpty() && txHash != "null") {
            int block = trace["blockNumber"].toInt();
            QString ts = trace["blockTimestampISO"].toString();
            if (ts.isEmpty()) ts = trace["blockTimestamp"].toString();
            ts = ts.left(19).replace("T", " ");

            m_resultTxHash->setText("Tx: " + txHash.left(20) + "");
            m_resultBlock->setText("Block: " + QString::number(block));
            m_resultTimestamp->setText(ts);
        } else {
            m_resultTxHash->setText("Tx: ");
            m_resultBlock->setText("Block: ");
            m_resultTimestamp->setText("");
        }
    } else {
        m_resultTxHash->setText("Tx: ");
        m_resultBlock->setText("Block: ");
        m_resultTimestamp->setText("");
    }
}
