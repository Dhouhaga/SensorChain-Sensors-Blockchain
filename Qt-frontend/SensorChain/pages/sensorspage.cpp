// sensorspage.cpp
#include "sensorspage.h"
#include <QHeaderView>
#include <QDateTime>

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

    // ── Left: submit reading panel ───────────────────────────────────
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->setSpacing(12);

    // Submit form
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
    QGroupBox *valueGroup = new QGroupBox("Temperature Value (°C)");
    QVBoxLayout *valueLayout = new QVBoxLayout(valueGroup);

    m_valueTempLabel = new QLabel("25.00 °C");
    m_valueTempLabel->setAlignment(Qt::AlignCenter);
    m_valueTempLabel->setStyleSheet("color: #4c6ef5; font-size: 22px; font-weight: bold;");

    m_valueSlider = new QSlider(Qt::Horizontal);
    m_valueSlider->setRange(-50, 100);  // -50.00 to 100.00 °C
    m_valueSlider->setValue(2500);
    m_valueSlider->setTickInterval(500);

    m_valueSpinBox = new QSpinBox();
    m_valueSpinBox->setRange(-5000, 10000);
    m_valueSpinBox->setValue(2500);
    m_valueSpinBox->setSuffix(" °C");

    QLabel *rangeLabel = new QLabel("-50.00 °C  ←  slider  →  100.00 °C");
    rangeLabel->setAlignment(Qt::AlignCenter);
    rangeLabel->setObjectName("pageSubtitle");

    valueLayout->addWidget(m_valueTempLabel);
    valueLayout->addWidget(m_valueSlider);
    valueLayout->addWidget(rangeLabel);
    valueLayout->addWidget(m_valueSpinBox);
    submitLayout->addWidget(valueGroup);

    // Firmware hash override
    QLabel *firmwareLbl = new QLabel("Firmware Hash (auto-filled, editable):");
    m_firmwareHashInput = new QLineEdit();
    m_firmwareHashInput->setPlaceholderText("0x1234...");
    submitLayout->addWidget(firmwareLbl);
    submitLayout->addWidget(m_firmwareHashInput);

    // Submit button
    m_submitBtn = new QPushButton("Submit Reading");
    m_submitBtn->setObjectName("primaryBtn");
    m_submitBtn->setMinimumHeight(42);
    submitLayout->addWidget(m_submitBtn);

    // Status label
    m_statusLabel = new QLabel("");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    submitLayout->addWidget(m_statusLabel);

    leftLayout->addWidget(submitGroup);

    // Result display
    QGroupBox *resultGroup = new QGroupBox("Last Submission Result");
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    m_resultDisplay = new QTextEdit();
    m_resultDisplay->setReadOnly(true);
    m_resultDisplay->setMaximumHeight(140);
    resultLayout->addWidget(m_resultDisplay);
    leftLayout->addWidget(resultGroup);

    // ── Right: current round + reading lookup ────────────────────────
    QWidget *rightWidget = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(12);

    // Current round participants
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

    // Reading lookup
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

    // Connect signals
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

    for (int i = 0; i < devices.size(); i++) {
        QJsonObject d = devices[i].toObject();
        if (!d["isActive"].toBool()) continue;

        QString addr = d["deviceAddress"].toString();
        QString type = d["deviceType"].toString();
        m_sensorDropdown->addItem(type + "  (" + addr.left(10) + "...)", addr);
        m_sensorFirmwareMap[addr] = d["firmwareHash"].toString();
    }

    if (m_sensorDropdown->count() > 0)
        onSensorSelected(0);
}

void SensorsPage::onSensorSelected(int index)
{
    if (index < 0) return;
    QString addr     = m_sensorDropdown->itemData(index).toString();
    QString firmware = m_sensorFirmwareMap.value(addr, "");
    m_sensorAddressLabel->setText(addr);
    m_sensorFirmwareLabel->setText(firmware.left(20) + "...");
    m_firmwareHashInput->setText(firmware);
}

void SensorsPage::onSliderChanged(int value)
{
    m_valueSpinBox->blockSignals(true);
    m_valueSpinBox->setValue(value);
    m_valueSpinBox->blockSignals(false);
    m_valueTempLabel->setText(QString::number(value, 'f', 2) + " °C");
}

void SensorsPage::onSpinChanged(int value)
{
    m_valueSlider->blockSignals(true);
    m_valueSlider->setValue(value);
    m_valueSlider->blockSignals(false);
    m_valueTempLabel->setText(QString::number(value, 'f', 2) + " °C");
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
    setStatus("Reading submitted successfully!", true);

    QString text;
    text += "Transaction confirmed\n";
    text += "Tx Hash:   " + receipt["transactionHash"].toString() + "\n";
    text += "Block:     " + receipt["blockNumber"].toString() + "\n";
    text += "Gas Used:  " + receipt["gasUsed"].toString() + "\n";
    text += "Sensor:    " + receipt["sensorAddress"].toString() + "\n";
    text += "Value:     " + receipt["valueScaled"].toString() + " °C\n";
    m_resultDisplay->setText(text);

    // Refresh round participants
    m_api->getCurrentRound();
}

void SensorsPage::onCurrentRound(QJsonObject data)
{
    QJsonArray details = data["participantDetails"].toArray();
    // Fallback to participants array if details not available
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

        QString displayVal = (val == "--" || val.isEmpty()) ? "--" : scaled + " °C";
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
    text += "Value:     " + reading["valueScaled"].toString() + " °C\n";
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
    setStatus(error, false);
    m_resultDisplay->setText("Error on " + endpoint + "\n" + error);
}

void SensorsPage::setStatus(const QString &msg, bool success)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(success
                                     ? "color: #2b8c4e; font-weight: bold;"
                                     : "color: #c92a2a; font-weight: bold;");
}
