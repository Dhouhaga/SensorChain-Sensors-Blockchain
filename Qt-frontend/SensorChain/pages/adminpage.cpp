#include "adminpage.h"
#include <QDateTime>
#include <QScrollArea>
#include <QScrollBar>
AdminPage::AdminPage(ApiClient *api, QWidget *parent)
    : QWidget(parent), m_api(api)
{
    setupUi();

    // Registry write
    connect(m_api, &ApiClient::deviceRegistered,   this, &AdminPage::onDeviceRegistered);
    connect(m_api, &ApiClient::firmwareUpdated,     this, &AdminPage::onFirmwareUpdated);
    connect(m_api, &ApiClient::deviceDeactivated,   this, &AdminPage::onDeviceDeactivated);
    connect(m_api, &ApiClient::deviceReactivated,   this, &AdminPage::onDeviceReactivated);
    // Consensus write
    connect(m_api, &ApiClient::newRoundStarted,     this, &AdminPage::onNewRoundStarted);
    connect(m_api, &ApiClient::consensusForced,     this, &AdminPage::onConsensusForced);
    connect(m_api, &ApiClient::faultyThresholdSet,  this, &AdminPage::onFaultyThresholdSet);
    connect(m_api, &ApiClient::minSensorsSet,       this, &AdminPage::onMinSensorsSet);
    connect(m_api, &ApiClient::consensusWindowSet,  this, &AdminPage::onConsensusWindowSet);
    connect(m_api, &ApiClient::deviceRegistrySet,   this, &AdminPage::onDeviceRegistrySet);
    // Stats
    connect(m_api, &ApiClient::registryStatsReady,  this, &AdminPage::onRegistryStats);
    connect(m_api, &ApiClient::consensusStatsReady, this, &AdminPage::onConsensusStats);
    // Error
    connect(m_api, &ApiClient::requestError,        this, &AdminPage::onError);

    refresh();
}

void AdminPage::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel *title = new QLabel("Admin");
    title->setObjectName("pageTitle");
    QLabel *subtitle = new QLabel("Contract owner functions — device management and system settings");
    subtitle->setObjectName("pageSubtitle");
    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    QHBoxLayout *contentRow = new QHBoxLayout();
    contentRow->setSpacing(16);

    // ── Left: tabs ───────────────────────────────────────────────────
    m_tabs = new QTabWidget();
    m_tabs->addTab(buildRegistryTab(),  "📋  Registry");
    m_tabs->addTab(buildConsensusTab(), "⚡  Consensus");
    m_tabs->addTab(buildInfoTab(),      "ℹ️  Info");
    contentRow->addWidget(m_tabs, 2);

    // ── Right: activity log ──────────────────────────────────────────
    QGroupBox *logGroup = new QGroupBox("Activity Log");
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_activityLog = new QTextEdit();
    m_activityLog->setReadOnly(true);
    logLayout->addWidget(m_activityLog);
    contentRow->addWidget(logGroup, 1);

    mainLayout->addLayout(contentRow, 1);
}

QWidget *AdminPage::buildRegistryTab()
{
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setSpacing(16);

    // Register device
    QGroupBox *regGroup = new QGroupBox("Register New Device");
    QFormLayout *regForm = new QFormLayout(regGroup);

    m_regAddressInput  = new QLineEdit(); m_regAddressInput->setPlaceholderText("0x...");
    m_regFirmwareInput = new QLineEdit(); m_regFirmwareInput->setPlaceholderText("0x1234...");
    m_regVersionInput  = new QSpinBox();  m_regVersionInput->setRange(1, 9999); m_regVersionInput->setValue(1);
    m_regTypeInput     = new QLineEdit(); m_regTypeInput->setPlaceholderText("Temperature Sensor");
    m_registerBtn      = new QPushButton("✅  Register Device");
    m_registerBtn->setObjectName("successBtn");

    regForm->addRow("Device Address:",   m_regAddressInput);
    regForm->addRow("Firmware Hash:",    m_regFirmwareInput);
    regForm->addRow("Firmware Version:", m_regVersionInput);
    regForm->addRow("Device Type:",      m_regTypeInput);
    regForm->addRow("",                  m_registerBtn);
    layout->addWidget(regGroup);

    // Update firmware
    QGroupBox *updGroup = new QGroupBox("Update Firmware");
    QFormLayout *updForm = new QFormLayout(updGroup);

    m_updAddressInput  = new QLineEdit(); m_updAddressInput->setPlaceholderText("0x...");
    m_updFirmwareInput = new QLineEdit(); m_updFirmwareInput->setPlaceholderText("0xNewHash...");
    m_updVersionInput  = new QSpinBox();  m_updVersionInput->setRange(1, 9999); m_updVersionInput->setValue(2);
    m_updateFirmwareBtn = new QPushButton("🔄  Update Firmware");
    m_updateFirmwareBtn->setObjectName("primaryBtn");

    updForm->addRow("Device Address:",   m_updAddressInput);
    updForm->addRow("New Firmware Hash:",m_updFirmwareInput);
    updForm->addRow("New Version:",      m_updVersionInput);
    updForm->addRow("",                  m_updateFirmwareBtn);
    layout->addWidget(updGroup);

    // Deactivate / Reactivate
    QGroupBox *actGroup = new QGroupBox("Deactivate / Reactivate Device");
    QFormLayout *actForm = new QFormLayout(actGroup);

    m_deactAddressInput = new QLineEdit(); m_deactAddressInput->setPlaceholderText("0x...");
    m_deactivateBtn     = new QPushButton("🔴  Deactivate");
    m_reactivateBtn     = new QPushButton("🟢  Reactivate");
    m_deactivateBtn->setObjectName("dangerBtn");
    m_reactivateBtn->setObjectName("successBtn");

    QHBoxLayout *actBtnRow = new QHBoxLayout();
    actBtnRow->addWidget(m_deactivateBtn);
    actBtnRow->addWidget(m_reactivateBtn);

    actForm->addRow("Device Address:", m_deactAddressInput);
    actForm->addRow("",                actBtnRow);
    layout->addWidget(actGroup);
    layout->addStretch();

    // Connect
    connect(m_registerBtn,      &QPushButton::clicked, this, &AdminPage::onRegisterDeviceClicked);
    connect(m_updateFirmwareBtn,&QPushButton::clicked, this, &AdminPage::onUpdateFirmwareClicked);
    connect(m_deactivateBtn,    &QPushButton::clicked, this, &AdminPage::onDeactivateClicked);
    connect(m_reactivateBtn,    &QPushButton::clicked, this, &AdminPage::onReactivateClicked);

    scroll->setWidget(container);
    return scroll;
}

QWidget *AdminPage::buildConsensusTab()
{
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setSpacing(16);

    // Round controls
    QGroupBox *roundGroup = new QGroupBox("Round Management");
    QHBoxLayout *roundLayout = new QHBoxLayout(roundGroup);

    m_forceNewRoundBtn   = new QPushButton("⏭  Force New Round");
    m_forceConsensusBtn  = new QPushButton("⚡  Force Consensus");
    m_forceNewRoundBtn->setObjectName("primaryBtn");
    m_forceConsensusBtn->setObjectName("primaryBtn");
    m_forceNewRoundBtn->setMinimumHeight(42);
    m_forceConsensusBtn->setMinimumHeight(42);

    roundLayout->addWidget(m_forceNewRoundBtn);
    roundLayout->addWidget(m_forceConsensusBtn);
    layout->addWidget(roundGroup);

    // Settings
    QGroupBox *settingsGroup = new QGroupBox("Consensus Settings");
    QFormLayout *settingsForm = new QFormLayout(settingsGroup);

    // Faulty threshold
    m_thresholdInput = new QSpinBox();
    m_thresholdInput->setRange(1, 99999);
    m_thresholdInput->setValue(500);
    m_thresholdInput->setSuffix("  (÷100 = °C)");
    m_setThresholdBtn = new QPushButton("Set");
    QHBoxLayout *threshRow = new QHBoxLayout();
    threshRow->addWidget(m_thresholdInput, 1);
    threshRow->addWidget(m_setThresholdBtn);
    settingsForm->addRow("Fault Threshold (×100):", threshRow);

    // Min sensors
    m_minSensorsInput = new QSpinBox();
    m_minSensorsInput->setRange(2, 100);
    m_minSensorsInput->setValue(3);
    m_setMinSensorsBtn = new QPushButton("Set");
    QHBoxLayout *minRow = new QHBoxLayout();
    minRow->addWidget(m_minSensorsInput, 1);
    minRow->addWidget(m_setMinSensorsBtn);
    settingsForm->addRow("Min Sensors:", minRow);

    // Consensus window
    m_windowInput = new QSpinBox();
    m_windowInput->setRange(60, 86400);
    m_windowInput->setValue(3600);
    m_windowInput->setSuffix("  seconds");
    m_setWindowBtn = new QPushButton("Set");
    QHBoxLayout *windowRow = new QHBoxLayout();
    windowRow->addWidget(m_windowInput, 1);
    windowRow->addWidget(m_setWindowBtn);
    settingsForm->addRow("Consensus Window:", windowRow);

    // Device registry address
    m_registryAddressInput = new QLineEdit();
    m_registryAddressInput->setPlaceholderText("0x...");
    m_setRegistryBtn = new QPushButton("Set");
    QHBoxLayout *regRow = new QHBoxLayout();
    regRow->addWidget(m_registryAddressInput, 1);
    regRow->addWidget(m_setRegistryBtn);
    settingsForm->addRow("Device Registry Address:", regRow);

    layout->addWidget(settingsGroup);
    layout->addStretch();

    // Connect
    connect(m_forceNewRoundBtn,  &QPushButton::clicked, this, &AdminPage::onForceNewRoundClicked);
    connect(m_forceConsensusBtn, &QPushButton::clicked, this, &AdminPage::onForceConsensusClicked);
    connect(m_setThresholdBtn,   &QPushButton::clicked, this, &AdminPage::onSetThresholdClicked);
    connect(m_setMinSensorsBtn,  &QPushButton::clicked, this, &AdminPage::onSetMinSensorsClicked);
    connect(m_setWindowBtn,      &QPushButton::clicked, this, &AdminPage::onSetWindowClicked);
    connect(m_setRegistryBtn,    &QPushButton::clicked, this, &AdminPage::onSetRegistryClicked);

    scroll->setWidget(container);
    return scroll;
}

QWidget *AdminPage::buildInfoTab()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setSpacing(16);

    auto makeLabel = [](const QString &text) {
        QLabel *l = new QLabel(text);
        l->setWordWrap(true);
        return l;
    };

    QGroupBox *regInfoGroup = new QGroupBox("DeviceRegistry Contract");
    QFormLayout *regInfoForm = new QFormLayout(regInfoGroup);
    m_registryOwnerLabel   = makeLabel("--");
    m_registryAddressLabel = makeLabel("--");
    m_totalDevicesLabel    = makeLabel("--");
    regInfoForm->addRow("Owner:",          m_registryOwnerLabel);
    regInfoForm->addRow("Address:",        m_registryAddressLabel);
    regInfoForm->addRow("Total Devices:",  m_totalDevicesLabel);
    layout->addWidget(regInfoGroup);

    QGroupBox *consInfoGroup = new QGroupBox("SensorConsensus Contract");
    QFormLayout *consInfoForm = new QFormLayout(consInfoGroup);
    m_consensusOwnerLabel   = makeLabel("--");
    m_consensusAddressLabel = makeLabel("--");
    m_consensusRegistryLabel= makeLabel("--");
    m_currentRoundIdLabel   = makeLabel("--");
    m_totalRoundsLabel      = makeLabel("--");
    m_minSensorsLabel       = makeLabel("--");
    m_thresholdLabel        = makeLabel("--");
    m_windowLabel           = makeLabel("--");
    consInfoForm->addRow("Owner:",              m_consensusOwnerLabel);
    consInfoForm->addRow("Address:",            m_consensusAddressLabel);
    consInfoForm->addRow("Linked Registry:",    m_consensusRegistryLabel);
    consInfoForm->addRow("Current Round:",      m_currentRoundIdLabel);
    consInfoForm->addRow("Total Rounds:",       m_totalRoundsLabel);
    consInfoForm->addRow("Min Sensors:",        m_minSensorsLabel);
    consInfoForm->addRow("Fault Threshold:",    m_thresholdLabel);
    consInfoForm->addRow("Consensus Window:",   m_windowLabel);
    layout->addWidget(consInfoGroup);
    layout->addStretch();

    return container;
}

void AdminPage::refresh()
{
    m_api->getRegistryStats();
    m_api->getConsensusStats();
}

void AdminPage::logResult(const QString &action, const QJsonObject &receipt, bool success)
{
    QDateTime now = QDateTime::currentDateTime();
    QString prefix = success ? "✅" : "❌";
    QString line = QString("[%1]  %2  %3\n")
        .arg(now.toString("hh:mm:ss"))
        .arg(prefix)
        .arg(action);

    if (!receipt.isEmpty()) {
        line += "     Tx: " + receipt["transactionHash"].toString() + "\n";
        line += "     Block: " + receipt["blockNumber"].toString()
              + "  |  Gas: " + receipt["gasUsed"].toString() + "\n";
    }
    line += "\n";
    m_activityLog->insertPlainText(line);
    m_activityLog->verticalScrollBar()->setValue(m_activityLog->verticalScrollBar()->maximum());
}

// ── Registry write responses ─────────────────────────────────────────────────

void AdminPage::onDeviceRegistered(QJsonObject r)   { logResult("Device Registered", r);  refresh(); }
void AdminPage::onFirmwareUpdated(QJsonObject r)     { logResult("Firmware Updated", r);   refresh(); }
void AdminPage::onDeviceDeactivated(QJsonObject r)   { logResult("Device Deactivated", r); refresh(); }
void AdminPage::onDeviceReactivated(QJsonObject r)   { logResult("Device Reactivated", r); refresh(); }

// ── Consensus write responses ─────────────────────────────────────────────────

void AdminPage::onNewRoundStarted(QJsonObject r)    { logResult("New Round Started", r);       refresh(); }
void AdminPage::onConsensusForced(QJsonObject r)    { logResult("Consensus Forced", r);        refresh(); }
void AdminPage::onFaultyThresholdSet(QJsonObject r) { logResult("Fault Threshold Updated", r); refresh(); }
void AdminPage::onMinSensorsSet(QJsonObject r)      { logResult("Min Sensors Updated", r);     refresh(); }
void AdminPage::onConsensusWindowSet(QJsonObject r) { logResult("Consensus Window Updated", r);refresh(); }
void AdminPage::onDeviceRegistrySet(QJsonObject r)  { logResult("Device Registry Updated", r); refresh(); }

// ── Stats ─────────────────────────────────────────────────────────────────────

void AdminPage::onRegistryStats(QJsonObject data)
{
    m_registryOwnerLabel->setText(data["contractOwner"].toString());
    m_registryAddressLabel->setText(data["contractAddress"].toString());
    m_totalDevicesLabel->setText(data["totalDevices"].toString());
}

void AdminPage::onConsensusStats(QJsonObject data)
{
    m_consensusOwnerLabel->setText(data["owner"].toString());
    m_consensusAddressLabel->setText(data["contractAddress"].toString());
    m_consensusRegistryLabel->setText(data["linkedRegistryAddress"].toString());
    m_currentRoundIdLabel->setText(data["currentRoundId"].toString());
    m_totalRoundsLabel->setText(data["totalRounds"].toString());
    m_minSensorsLabel->setText(data["minSensorsForConsensus"].toString());
    m_thresholdLabel->setText(data["faultyThresholdUnits"].toString()
                              + "  (" + data["faultyThresholdScaled"].toString() + "°)");
    m_windowLabel->setText(data["consensusWindow"].toString() + " seconds");

    // Pre-fill settings inputs with current values
    m_thresholdInput->setValue(data["faultyThresholdUnits"].toString("500").toInt());
    m_minSensorsInput->setValue(data["minSensorsForConsensus"].toString("3").toInt());
    m_windowInput->setValue(data["consensusWindow"].toString("3600").toInt());
    m_registryAddressInput->setText(data["linkedRegistryAddress"].toString());
}

void AdminPage::onError(QString endpoint, QString error)
{
    logResult("ERROR on " + endpoint + ": " + error, {}, false);
}

// ── Button handlers ──────────────────────────────────────────────────────────

void AdminPage::onRegisterDeviceClicked() {
    m_api->registerDevice(
        m_regAddressInput->text().trimmed(),
        m_regFirmwareInput->text().trimmed(),
        m_regVersionInput->value(),
        m_regTypeInput->text().trimmed()
    );
}

void AdminPage::onUpdateFirmwareClicked() {
    m_api->updateFirmware(
        m_updAddressInput->text().trimmed(),
        m_updFirmwareInput->text().trimmed(),
        m_updVersionInput->value()
    );
}

void AdminPage::onDeactivateClicked() {
    m_api->deactivateDevice(m_deactAddressInput->text().trimmed());
}

void AdminPage::onReactivateClicked() {
    m_api->reactivateDevice(m_deactAddressInput->text().trimmed());
}

void AdminPage::onForceNewRoundClicked()  { m_api->forceNewRound(); }
void AdminPage::onForceConsensusClicked() { m_api->forceConsensus(); }

void AdminPage::onSetThresholdClicked() {
    m_api->setFaultyThreshold(m_thresholdInput->value());
}

void AdminPage::onSetMinSensorsClicked() {
    m_api->setMinSensors(m_minSensorsInput->value());
}

void AdminPage::onSetWindowClicked() {
    m_api->setConsensusWindow(m_windowInput->value());
}

void AdminPage::onSetRegistryClicked() {
    m_api->setDeviceRegistry(m_registryAddressInput->text().trimmed());
}
