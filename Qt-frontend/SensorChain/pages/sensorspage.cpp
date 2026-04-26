#include "sensorspage.h"
#include <QHeaderView>
#include <QDateTime>
#include <qdialog.h>

SensorsPage::SensorsPage(ApiClient *api, QWidget *parent)
    : QWidget(parent), m_api(api)
{
    setupUi();

    connect(m_api, &ApiClient::allDevicesReady,    this, &SensorsPage::onAllDevices);
    connect(m_api, &ApiClient::readingSubmitted,   this, &SensorsPage::onReadingSubmitted);
    connect(m_api, &ApiClient::currentRoundReady,  this, &SensorsPage::onCurrentRound);
    connect(m_api, &ApiClient::readingReady,       this, &SensorsPage::onReadingReady);
    connect(m_api, &ApiClient::requestError,       this, &SensorsPage::onError);

    refresh();
}

void SensorsPage::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel *title = new QLabel("Sensors");
    title->setObjectName("pageTitle");
    QLabel *subtitle = new QLabel("Submit sensor readings and monitor current round");
    subtitle->setObjectName("pageSubtitle");
    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    QHBoxLayout *contentRow = new QHBoxLayout();
    contentRow->setSpacing(16);

    //  Left: submit reading panel 
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->setSpacing(12);

    QGroupBox *submitGroup = new QGroupBox("Submit Reading");
    QVBoxLayout *submitLayout = new QVBoxLayout(submitGroup);

    QFormLayout *formLayout = new QFormLayout();

    m_sensorDropdown = new QComboBox();
    formLayout->addRow("Sensor:", m_sensorDropdown);

    m_sensorAddressLabel = new QLabel("--");
    m_sensorAddressLabel->setObjectName("pageSubtitle");
    m_sensorAddressLabel->setWordWrap(true);
    formLayout->addRow("Address:", m_sensorAddressLabel);

    m_sensorFirmwareLabel = new QLabel("--");
    m_sensorFirmwareLabel->setObjectName("pageSubtitle");
    m_sensorFirmwareLabel->setWordWrap(true);
    formLayout->addRow("Firmware:", m_sensorFirmwareLabel);

    submitLayout->addLayout(formLayout);

    // Value slider + spinbox
    QGroupBox *valueGroup = new QGroupBox("Temperature Value (C)");
    QVBoxLayout *valueLayout = new QVBoxLayout(valueGroup);

    m_valueTempLabel = new QLabel("25.00 C");
    m_valueTempLabel->setAlignment(Qt::AlignCenter);
    m_valueTempLabel->setStyleSheet("color: #4c6ef5; font-size: 22px; font-weight: bold;");

    m_valueSlider = new QSlider(Qt::Horizontal);
    m_valueSlider->setRange(-50, 100);  // -50.00 to 100.00 C
    m_valueSlider->setValue(2500);
    m_valueSlider->setTickInterval(500);

    m_valueSpinBox = new QSpinBox();
    m_valueSpinBox->setRange(-5000, 10000);
    m_valueSpinBox->setValue(2500);
    m_valueSpinBox->setSuffix(" C");

    QLabel *rangeLabel = new QLabel("-50.00 C    slider    100.00 C");
    rangeLabel->setAlignment(Qt::AlignCenter);
    rangeLabel->setObjectName("pageSubtitle");

    valueLayout->addWidget(m_valueTempLabel);
    valueLayout->addWidget(m_valueSlider);
    valueLayout->addWidget(rangeLabel);
    valueLayout->addWidget(m_valueSpinBox);
    submitLayout->addWidget(valueGroup);

    QLabel *firmwareLbl = new QLabel("Firmware Hash (auto-filled, editable):");
    m_firmwareHashInput = new QLineEdit();
    m_firmwareHashInput->setPlaceholderText("0x1234...");
    submitLayout->addWidget(firmwareLbl);
    submitLayout->addWidget(m_firmwareHashInput);

    m_submitBtn = new QPushButton("Submit Reading");
    m_submitBtn->setObjectName("primaryBtn");
    m_submitBtn->setMinimumHeight(42);
    submitLayout->addWidget(m_submitBtn);

    m_statusLabel = new QLabel("");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    submitLayout->addWidget(m_statusLabel);

    leftLayout->addWidget(submitGroup);

    QGroupBox *resultGroup = new QGroupBox("Last Submission Result");
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    m_resultDisplay = new QTextEdit();
    m_resultDisplay->setReadOnly(true);
    m_resultDisplay->setMaximumHeight(140);
    resultLayout->addWidget(m_resultDisplay);
    leftLayout->addWidget(resultGroup);

    //  Right: current round + reading lookup 
    QWidget *rightWidget = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(12);

    QGroupBox *roundGroup = new QGroupBox("Current Round Participants");
    QVBoxLayout *roundLayout = new QVBoxLayout(roundGroup);

    m_participantsTable = new QTableWidget(0, 3);
    m_participantsTable->setHorizontalHeaderLabels({"Address", "Value", "Status"});
    m_participantsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_participantsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_participantsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_participantsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_participantsTable->setFocusPolicy(Qt::NoFocus);
    roundLayout->addWidget(m_participantsTable);
    rightLayout->addWidget(roundGroup);

    QGroupBox *lookupGroup = new QGroupBox("Lookup Specific Reading");
    QFormLayout *lookupForm = new QFormLayout(lookupGroup);

    m_lookupRoundSpinBox = new QSpinBox();
    m_lookupRoundSpinBox->setRange(1, 99999);
    m_lookupRoundSpinBox->setValue(1);

    m_lookupSensorInput = new QLineEdit();
    m_lookupSensorInput->setPlaceholderText("0x...");

    m_lookupBtn = new QPushButton("Lookup");
    m_lookupBtn->setObjectName("primaryBtn");

    m_lookupDisplay = new QTextEdit();
    m_lookupDisplay->setReadOnly(true);
    m_lookupDisplay->setMaximumHeight(120);

    lookupForm->addRow("Round ID:", m_lookupRoundSpinBox);
    lookupForm->addRow("Sensor Address:", m_lookupSensorInput);
    lookupForm->addRow("", m_lookupBtn);
    lookupForm->addRow("Result:", m_lookupDisplay);

    rightLayout->addWidget(lookupGroup);
    rightLayout->addStretch();

    contentRow->addWidget(leftWidget, 1);
    contentRow->addWidget(rightWidget, 1);
    mainLayout->addLayout(contentRow, 1);

    connect(m_submitBtn,    &QPushButton::clicked,          this, &SensorsPage::onSubmitClicked);
    connect(m_lookupBtn,    &QPushButton::clicked,          this, &SensorsPage::onLookupReadingClicked);
    connect(m_sensorDropdown, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SensorsPage::onSensorSelected);
    connect(m_valueSlider,  &QSlider::valueChanged,         this, &SensorsPage::onSliderChanged);
    connect(m_valueSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SensorsPage::onSpinChanged);
}

void SensorsPage::refresh()
{
    m_api->getAllDevices();
    m_api->getCurrentRound();
}

void SensorsPage::onAllDevices(QJsonArray devices)
{
    populateSensorDropdown(devices);
}

void SensorsPage::populateSensorDropdown(const QJsonArray &devices)
{
    m_sensorDropdown->clear();
    m_sensorFirmwareMap.clear();
    m_sensorActiveMap.clear();  // Clear the active status map

    for (int i = 0; i < devices.size(); i++) {
        QJsonObject d = devices[i].toObject();

        QString addr = d["deviceAddress"].toString();
        QString type = d["deviceType"].toString();
        bool isActive = d["isActive"].toBool();
        QString firmwareHash = d["firmwareHash"].toString();

        m_sensorFirmwareMap[addr] = firmwareHash;
        m_sensorActiveMap[addr] = isActive;

        QString displayName = type + "  (" + addr.left(10) + "...)";
        if (!isActive) {
            displayName += " [INACTIVE]";
        }

        m_sensorDropdown->addItem(displayName, addr);
    }

    if (m_sensorDropdown->count() > 0)
        onSensorSelected(0);
}

void SensorsPage::onSensorSelected(int index)
{
    if (index < 0) return;

    QString addr = m_sensorDropdown->itemData(index).toString();
    QString firmware = m_sensorFirmwareMap.value(addr, "");
    bool isActive = m_sensorActiveMap.value(addr, false);

    m_sensorAddressLabel->setText(addr);
    m_sensorFirmwareLabel->setText(firmware.left(20) + "...");
    m_firmwareHashInput->setText(firmware);

    if (!isActive) {
        m_statusLabel->setText(" WARNING: This sensor is DEACTIVATED on the blockchain. Submission will FAIL!");
        m_statusLabel->setStyleSheet("color: #e67700; font-weight: bold;");

    } else {
        m_statusLabel->setText(" Sensor active and ready to submit");
        m_statusLabel->setStyleSheet("color: #2b8c4e; font-weight: bold;");
        m_submitBtn->setEnabled(true);
    }
}

void SensorsPage::onSliderChanged(int value)
{
    m_valueSpinBox->blockSignals(true);
    m_valueSpinBox->setValue(value);
    m_valueSpinBox->blockSignals(false);
    m_valueTempLabel->setText(QString::number(value, 'f', 2) + " C");
}

void SensorsPage::onSpinChanged(int value)
{
    m_valueSlider->blockSignals(true);
    m_valueSlider->setValue(value);
    m_valueSlider->blockSignals(false);
    m_valueTempLabel->setText(QString::number(value, 'f', 2) + " C");
}

void SensorsPage::onSubmitClicked()
{
    QString addr     = m_sensorDropdown->currentData().toString();
    int value = m_valueSpinBox->value() * 100;
    QString firmware = m_firmwareHashInput->text().trimmed();

    if (addr.isEmpty() || firmware.isEmpty()) {
        setStatus("Please select a sensor and provide a firmware hash", false);
        return;
    }

    m_submitBtn->setEnabled(false);
    m_submitBtn->setText("Submitting...");
    setStatus("Sending to blockchain...", true);

    m_api->submitReading(addr, value, firmware);
}

void SensorsPage::onReadingSubmitted(QJsonObject receipt)
{
    m_submitBtn->setEnabled(true);
    m_submitBtn->setText("Submit Reading");

    // Check if this is an error response (success: false)
    if (receipt.contains("success") && !receipt["success"].toBool()) {
        QString errorMsg = receipt["error"].toString();
        QString actionMsg = receipt["action"].toString();
        QJsonObject details = receipt["details"].toObject();
        QString reason = details["reason"].toString();
        QString txHash = details["transactionHash"].toString();

        QString text;
        text += " SUBMISSION FAILED\n";
        text += "\n";
        text += "Error: " + errorMsg + "\n\n";

        text += "DETAILS:\n";
        text += "\n";
        text += "What happened on the blockchain?\n";
        text += " Transaction was submitted to the network\n";
        text += " Transaction was MINED (included in a block)\n";
        text += " During execution, the contract REVERTED the transaction\n";
        text += " All state changes were rolled back\n";
        text += " Gas was still consumed (miner compensated for computation)\n\n";

        text += "Revert Reason: " + reason + "\n";
        text += "Transaction Hash: " + txHash + "\n";
        text += "Status: REVERTED (not successful)\n\n";

        text += "Why this happened:\n";
        if (reason.contains("not registered or inactive")) {
            text += " This sensor address is not in the DeviceRegistry OR\n";
            text += " The sensor has been deactivated by the contract owner\n";
            text += " Only active, registered sensors can submit readings\n";
        } else if (reason.contains("Firmware hash mismatch")) {
            text += " The firmware hash provided doesn't match the on-chain record\n";
            text += " Device may be running unauthorized/outdated firmware\n";
        } else if (reason.contains("signature")) {
            text += " The cryptographic signature was invalid\n";
            text += " Reading was not properly signed by the sensor's private key\n";
        } else if (reason.contains("already submitted")) {
            text += " This sensor already submitted a reading for this round\n";
            text += " Only one submission per sensor per consensus round\n";
        }

        text += "\n" + actionMsg + "\n";
        text += "\n";
        m_resultDisplay->setText(text);
        setStatus(errorMsg, false);

        showTransactionFailureDialog(txHash, reason, errorMsg);
        return;
    }

    QJsonObject data = receipt;
    if (receipt.contains("data") && receipt["data"].isObject()) {
        data = receipt["data"].toObject();
    }

    QString txHash = data["transactionHash"].toString();
    if (txHash.isEmpty() || txHash == "0" || txHash == "unknown") {
        QString errorMsg = data["error"].toString();
        if (errorMsg.isEmpty()) {
            errorMsg = "Unknown error - no transaction hash";
        }

        QString text;
        text += " SUBMISSION FAILED\n";
        text += "\n";
        text += "Error: " + errorMsg + "\n";
        text += "\n";
        m_resultDisplay->setText(text);
        setStatus(errorMsg, false);
        return;
    }

    setStatus(" Reading submitted successfully!", true);

    QString text;
    text += " TRANSACTION CONFIRMED\n";
    text += "\n";
    text += "Tx Hash:   " + txHash + "\n";
    text += "Block:     " + data["blockNumber"].toString() + "\n";
    text += "Gas Used:  " + data["gasUsed"].toString() + "\n";
    text += "Sensor:    " + data["sensorAddress"].toString() + "\n";
    text += "Value:     " + data["valueScaled"].toString() + " C\n";
    text += "\n";
    text += " Reading stored on blockchain\n";
    text += " Waiting for consensus...\n";
    m_resultDisplay->setText(text);

    showTransactionSuccessDialog(txHash, data["blockNumber"].toInt(),
                                 data["gasUsed"].toString(), data["sensorAddress"].toString(),
                                 data["valueScaled"].toString());

    m_api->getCurrentRound();
}
void SensorsPage::showTransactionFailureDialog(const QString &txHash, const QString &reason, const QString &errorMsg)
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Transaction Failed");
    dialog->setMinimumWidth(500);

    QVBoxLayout *layout = new QVBoxLayout(dialog);

    QLabel *titleLabel = new QLabel("TRANSACTION REVERTED");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #c92a2a;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QTextEdit *details = new QTextEdit();
    details->setReadOnly(true);
    details->setMinimumHeight(400);

    QString text;
    text += "TRANSACTION SUMMARY\n";
    text += "\n";
    text += "Status:               REVERTED (Failed)\n";
    text += "Transaction Hash:    " + txHash + "\n";
    text += "Revert Reason:       " + reason + "\n";
    text += "User Error:          " + errorMsg + "\n\n";

    text += "WHAT HAPPENED ON THE BLOCKCHAIN\n";
    text += "\n";
    text += "1. Transaction submitted to the mempool\n";
    text += "2. Miner included transaction in a block\n";
    text += "3. EVM began executing submitReading() function\n";
    text += "4. Contract performed security checks:\n";
    text += "    verifyDevice() called on DeviceRegistry\n";
    text += "    firmware hash verification\n";
    text += "    signature verification\n";
    text += "5.  Security check FAILED at: " + reason + "\n";
    text += "6. Contract executed REVERT opcode\n";
    text += "7. All state changes rolled back\n";
    text += "8. Gas consumed (miner compensated for computation)\n";
    text += "9. Transaction marked as 'reverted' in block\n\n";

    details->setText(text);
    layout->addWidget(details);

    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setObjectName("primaryBtn");
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog->exec();
    delete dialog;
}

void SensorsPage::showTransactionSuccessDialog(const QString &txHash, int blockNumber, const QString &gasUsed,
                                               const QString &sensorAddress, const QString &value)
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Transaction Confirmed");
    dialog->setMinimumWidth(500);

    QVBoxLayout *layout = new QVBoxLayout(dialog);

    QLabel *titleLabel = new QLabel("TRANSACTION SUCCESSFUL");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #2b8c4e;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QTextEdit *details = new QTextEdit();
    details->setReadOnly(true);
    details->setMinimumHeight(350);

    QString text;
    text += "\n";
    text += "TRANSACTION SUMMARY\n";
    text += "\n\n";
    text += "Status:               SUCCESS\n";
    text += "Transaction Hash:    " + txHash + "\n";
    text += "Block Number:        " + QString::number(blockNumber) + "\n";
    text += "Gas Used:            " + gasUsed + "\n";
    text += "Sensor Address:      " + sensorAddress + "\n";
    text += "Submitted Value:     " + value + " C\n\n";

    text += "WHAT HAPPENED ON THE BLOCKCHAIN\n";
    text += "1. Transaction submitted to the mempool\n";
    text += "2. Miner included transaction in a block\n";
    text += "3. EVM executed submitReading() function\n";
    text += "4. Security checks PASSED:\n";
    text += "    Device is registered and ACTIVE\n";
    text += "    Firmware hash matches on-chain record\n";
    text += "    Cryptographic signature is valid\n";
    text += "    No duplicate submission for this round\n";
    text += "5. Reading stored in roundReadings mapping\n";
    text += "6. Event 'ReadingSubmitted' emitted\n";
    text += "7. Transaction committed to blockchain\n\n";

    text += " WHAT HAPPENS NEXT\n";
    text += " This reading is now stored on-chain\n";
    text += " When enough sensors submit OR time window expires:\n";
    text += "   Pairwise disagreement scoring will run\n";
    text += "   Faulty sensors will be detected\n";
    text += "   Trusted average will be calculated\n";
    text += "   ConsensusReached event will be emitted\n\n";

    text += "VERIFICATION\n";
    text += "You can inspect this transaction:\n";
    text += " Copy the Transaction Hash above\n";
    text += " Go to Blockchain Explorer tab\n";
    text += " Search for this hash to see full details\n";
    text += " Check the 'ReadingSubmitted' event in logs\n";

    details->setText(text);
    layout->addWidget(details);

    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setObjectName("primaryBtn");
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog->exec();
    delete dialog;
}

void SensorsPage::onCurrentRound(QJsonObject data)
{
    QJsonArray details = data["participantDetails"].toArray();
    if (details.isEmpty()) {
        details = QJsonArray();
        QJsonArray participants = data["participants"].toArray();
        for (int i = 0; i < participants.size(); i++) {
            QJsonObject d;
            d["address"]     = participants[i].toString();
            d["value"]       = "--";
            d["valueScaled"] = "--";
            d["isFaulty"]    = false;
            details.append(d);
        }
    }

    m_participantsTable->setRowCount(0);
    for (int i = 0; i < details.size(); i++) {
        QJsonObject d = details[i].toObject();
        QString addr    = d["address"].toString();
        QString val     = d["value"].toString();
        QString scaled  = d["valueScaled"].toString();
        bool isFaulty   = d["isFaulty"].toBool();

        m_participantsTable->insertRow(i);
        m_participantsTable->setItem(i, 0, new QTableWidgetItem(addr));

        QString displayVal = (val == "--" || val.isEmpty()) ? "--" : scaled + " C";
        m_participantsTable->setItem(i, 1, new QTableWidgetItem(displayVal));

        QTableWidgetItem *statusItem;
        if (isFaulty) {
            statusItem = new QTableWidgetItem("Faulty");
            statusItem->setForeground(QColor("#c92a2a"));
        } else {
            statusItem = new QTableWidgetItem("Submitted");
            statusItem->setForeground(QColor("#2b8c4e"));
        }
        m_participantsTable->setItem(i, 2, statusItem);
    }
}

void SensorsPage::onReadingReady(QJsonObject reading)
{
    QString text;
    text += "=== Reading ===\n";
    text += "Sensor:    " + reading["sensor"].toString() + "\n";
    text += "Value:     " + reading["valueScaled"].toString() + " C\n";
    text += "Timestamp: " + reading["timestamp"].toString() + "\n";
    text += "Faulty:    " + QString(reading["isFaulty"].toBool() ? "YES" : "NO") + "\n";
    m_lookupDisplay->setText(text);
}

void SensorsPage::onLookupReadingClicked()
{
    int     roundId = m_lookupRoundSpinBox->value();
    QString sensor  = m_lookupSensorInput->text().trimmed();
    if (!sensor.isEmpty()) m_api->getReading(roundId, sensor);
}

void SensorsPage::onError(QString endpoint, QString error)
{
    m_submitBtn->setEnabled(true);
    m_submitBtn->setText("Submit Reading");

    QString text;
    text += " REQUEST FAILED\n";
    text += "\n";
    text += "Endpoint: " + endpoint + "\n";
    text += "Error: " + error + "\n";
    text += "\n";

    m_resultDisplay->setText(text);
    setStatus(error, false);
}

void SensorsPage::setStatus(const QString &msg, bool success)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(success
                                     ? "color: #2b8c4e; font-weight: bold;"
                                     : "color: #c92a2a; font-weight: bold;");
}
