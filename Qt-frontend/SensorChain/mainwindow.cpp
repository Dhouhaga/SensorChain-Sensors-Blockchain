#include "mainwindow.h"
#include "pages/dashboardpage.h"
#include "pages/devicespage.h"
#include "pages/sensorspage.h"
#include "pages/adminpage.h"
#include "pages/historypage.h"
#include "pages/blockchainexplorerpage.h"   // NEW
#include "pages/consensusanalysispage.h"    // NEW
#include <QListWidgetItem>
#include <QJsonDocument>

// ============================================================
// TxFeedbackDialog
// ============================================================

TxFeedbackDialog::TxFeedbackDialog(const QString &operation,
                                    const QJsonObject &txData,
                                    QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Transaction Result — " + operation);
    setMinimumSize(600, 380);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    // Status header
    bool ok = txData["status"].toString() == "success";
    auto *statusLabel = new QLabel(ok ? "✅  Transaction Confirmed" : "❌  Transaction Failed");
    statusLabel->setStyleSheet(
        QString("font-size: 16px; font-weight: bold; color: %1;")
            .arg(ok ? "#51cf66" : "#ff6b6b")
    );
    layout->addWidget(statusLabel);

    // Key fields grid
    auto addRow = [&](const QString &label, const QString &value, bool mono = false) {
        auto *row   = new QHBoxLayout();
        auto *lbl   = new QLabel(label + ":");
        lbl->setFixedWidth(160);
        lbl->setStyleSheet("color: #8b8fa8; font-size: 12px;");
        auto *val   = new QLabel(value);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        val->setWordWrap(true);
        val->setStyleSheet(
            mono ? "color: #7c83fd; font-family: Consolas, monospace; font-size: 11px;"
                 : "color: #c0c4dc; font-size: 12px;"
        );
        row->addWidget(lbl);
        row->addWidget(val, 1);
        layout->addLayout(row);
    };

    addRow("Operation",       operation);
    addRow("Tx Hash",         txData["transactionHash"].toString(), true);
    addRow("Block Number",    QString::number(txData["blockNumber"].toInt()));
    addRow("Block Hash",      txData["blockHash"].toString(), true);
    addRow("Block Time",      txData["blockTimestampISO"].toString().replace("T"," ").left(19));
    addRow("From",            txData["from"].toString(), true);
    addRow("To",              txData["to"].toString(), true);
    addRow("Gas Used",        txData["gasUsed"].toString());
    addRow("Gas Price (Wei)", txData["gasPrice"].toString());
    addRow("Logs Count",      QString::number(txData["logsCount"].toInt()));

    layout->addStretch();

    // Hint
    auto *hint = new QLabel("💡 Copy the Tx Hash and paste it into the Blockchain Explorer tab to inspect decoded logs.");
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #4a4f6e; font-size: 11px; font-style: italic;");
    layout->addWidget(hint);

    // Close button
    auto *closeBtn = new QPushButton("Close");
    closeBtn->setObjectName("primaryBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    // Apply dark theme
    setStyleSheet(R"(
        QDialog { background-color: #1a1d2e; }
        QPushButton#primaryBtn {
            background-color: #7c83fd; color: #ffffff;
            border: none; border-radius: 6px;
            padding: 7px 20px; font-weight: bold;
        }
        QPushButton#primaryBtn:hover { background-color: #9198ff; }
    )");
}

// ============================================================
// MainWindow
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_api(new ApiClient(this))
{
    setWindowTitle("SensorChain — IoT Blockchain Dashboard");
    setMinimumSize(1200, 750);
    resize(1440, 880);

    setupUi();
    setupStyles();
    setupGlobalConnections();

    m_navList->setCurrentRow(0);
}

MainWindow::~MainWindow() {}

// ============================================================
// UI setup
// ============================================================

void MainWindow::setupUi()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_mainLayout = new QHBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // ── Side navigation panel ────────────────────────────────
    m_navPanel = new QWidget();
    m_navPanel->setFixedWidth(220);
    m_navPanel->setObjectName("navPanel");

    m_navLayout = new QVBoxLayout(m_navPanel);
    m_navLayout->setContentsMargins(0, 0, 0, 0);
    m_navLayout->setSpacing(0);

    QWidget *logoArea = new QWidget();
    logoArea->setObjectName("logoArea");
    logoArea->setFixedHeight(80);
    QVBoxLayout *logoLayout = new QVBoxLayout(logoArea);
    logoLayout->setAlignment(Qt::AlignCenter);

    m_logoLabel = new QLabel("⛓ SensorChain");
    m_logoLabel->setObjectName("logoLabel");
    m_logoLabel->setAlignment(Qt::AlignCenter);
    logoLayout->addWidget(m_logoLabel);

    QLabel *subtitleLabel = new QLabel("IoT Blockchain Monitor");
    subtitleLabel->setObjectName("logoSubtitle");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    logoLayout->addWidget(subtitleLabel);

    m_navLayout->addWidget(logoArea);

    QFrame *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("navSeparator");
    m_navLayout->addWidget(sep);

    m_navList = new QListWidget();
    m_navList->setObjectName("navList");
    m_navList->setFrameShape(QFrame::NoFrame);

    addNavItem("📊", "Dashboard");
    addNavItem("🔌", "Devices");
    addNavItem("📡", "Sensors");
    addNavItem("⚙️",  "Admin");
    addNavItem("📜", "History");
    addNavItem("⛓",  "Blockchain");    // NEW — index 5
    addNavItem("🔬", "Analysis");      // NEW — index 6

    m_navLayout->addWidget(m_navList, 1);

    m_versionLabel = new QLabel("v1.1.0  |  Ganache Local");
    m_versionLabel->setObjectName("versionLabel");
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_navLayout->addWidget(m_versionLabel);

    // ── Content pages ────────────────────────────────────────
    m_pages = new QStackedWidget();
    m_pages->setObjectName("contentArea");

    m_dashboardPage  = new DashboardPage(m_api, this);
    m_devicesPage    = new DevicesPage(m_api, this);
    m_sensorsPage    = new SensorsPage(m_api, this);
    m_adminPage      = new AdminPage(m_api, this);
    m_historyPage    = new HistoryPage(m_api, this);
    m_blockchainPage = new BlockchainExplorerPage(m_api, this);   // NEW
    m_analysisPage   = new ConsensusAnalysisPage(m_api, this);    // NEW

    m_pages->addWidget(m_dashboardPage);   // 0
    m_pages->addWidget(m_devicesPage);     // 1
    m_pages->addWidget(m_sensorsPage);     // 2
    m_pages->addWidget(m_adminPage);       // 3
    m_pages->addWidget(m_historyPage);     // 4
    m_pages->addWidget(m_blockchainPage);  // 5
    m_pages->addWidget(m_analysisPage);    // 6

    m_mainLayout->addWidget(m_navPanel);
    m_mainLayout->addWidget(m_pages, 1);

    connect(m_navList, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavItemChanged);
}

void MainWindow::addNavItem(const QString &icon, const QString &label)
{
    QListWidgetItem *item = new QListWidgetItem(icon + "  " + label);
    item->setSizeHint(QSize(220, 50));
    item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_navList->addItem(item);
}

// ============================================================
// Global write-result connections
// ============================================================

void MainWindow::setupGlobalConnections()
{
    // Every write operation → show TxFeedbackDialog
    connect(m_api, &ApiClient::deviceRegistered,
            this,  &MainWindow::onDeviceRegistered);
    connect(m_api, &ApiClient::firmwareUpdated,
            this,  &MainWindow::onFirmwareUpdated);
    connect(m_api, &ApiClient::deviceDeactivated,
            this,  &MainWindow::onDeviceDeactivated);
    connect(m_api, &ApiClient::deviceReactivated,
            this,  &MainWindow::onDeviceReactivated);
    connect(m_api, &ApiClient::readingSubmitted,
            this,  &MainWindow::onReadingSubmitted);
    connect(m_api, &ApiClient::newRoundStarted,
            this,  &MainWindow::onNewRoundStarted);
    connect(m_api, &ApiClient::consensusForced,
            this,  &MainWindow::onConsensusForced);
    connect(m_api, &ApiClient::faultyThresholdSet,
            this,  &MainWindow::onFaultyThresholdSet);
    connect(m_api, &ApiClient::minSensorsSet,
            this,  &MainWindow::onMinSensorsSet);
    connect(m_api, &ApiClient::consensusWindowSet,
            this,  &MainWindow::onConsensusWindowSet);
    connect(m_api, &ApiClient::deviceRegistrySet,
            this,  &MainWindow::onDeviceRegistrySet);
}

// ============================================================
// Navigation slot
// ============================================================

void MainWindow::onNavItemChanged(int index)
{
    m_pages->setCurrentIndex(index);
    switch (index) {
    case 0: m_dashboardPage->refresh();  break;
    case 1: m_devicesPage->refresh();    break;
    case 2: m_sensorsPage->refresh();    break;
    case 3: m_adminPage->refresh();      break;
    case 4: m_historyPage->refresh();    break;
    case 5: m_blockchainPage->refresh(); break;   // NEW
    case 6: m_analysisPage->refresh();   break;   // NEW
    }
}

// ============================================================
// Write result slots — each calls showTxFeedback
// ============================================================

void MainWindow::showTxFeedback(const QString &operation, const QJsonObject &txData)
{
    TxFeedbackDialog dlg(operation, txData, this);
    dlg.exec();
}

void MainWindow::onDeviceRegistered(QJsonObject txData) {
    showTxFeedback("Register Device", txData);
}
void MainWindow::onFirmwareUpdated(QJsonObject txData) {
    showTxFeedback("Update Firmware", txData);
}
void MainWindow::onDeviceDeactivated(QJsonObject txData) {
    showTxFeedback("Deactivate Device", txData);
}
void MainWindow::onDeviceReactivated(QJsonObject txData) {
    showTxFeedback("Reactivate Device", txData);
}
void MainWindow::onReadingSubmitted(QJsonObject txData) {
    showTxFeedback("Submit Reading", txData);
}
void MainWindow::onNewRoundStarted(QJsonObject txData) {
    showTxFeedback("Force New Round", txData);
}
void MainWindow::onConsensusForced(QJsonObject txData) {
    showTxFeedback("Force Consensus", txData);
}
void MainWindow::onFaultyThresholdSet(QJsonObject txData) {
    showTxFeedback("Set Faulty Threshold", txData);
}
void MainWindow::onMinSensorsSet(QJsonObject txData) {
    showTxFeedback("Set Min Sensors", txData);
}
void MainWindow::onConsensusWindowSet(QJsonObject txData) {
    showTxFeedback("Set Consensus Window", txData);
}
void MainWindow::onDeviceRegistrySet(QJsonObject txData) {
    showTxFeedback("Set Device Registry", txData);
}

// ============================================================
// Styles  (unchanged from v1.0, extended below)
// ============================================================

void MainWindow::setupStyles()
{
    setStyleSheet(R"(
        QMainWindow { background-color: #0f1117; }

        QWidget#navPanel {
            background-color: #1a1d2e;
            border-right: 1px solid #2d3250;
        }
        QWidget#logoArea { background-color: #1a1d2e; padding: 10px; }
        QLabel#logoLabel {
            color: #7c83fd; font-size: 16px;
            font-weight: bold; font-family: 'Segoe UI', Arial, sans-serif;
        }
        QLabel#logoSubtitle {
            color: #4a4f6e; font-size: 10px;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QFrame#navSeparator {
            color: #2d3250; background-color: #2d3250;
            border: none; height: 1px;
        }
        QListWidget#navList {
            background-color: #1a1d2e; color: #8b8fa8;
            font-size: 13px; font-family: 'Segoe UI', Arial, sans-serif;
            border: none; padding: 8px 0px;
        }
        QListWidget#navList::item {
            padding: 8px 20px; margin: 2px 8px; border-radius: 6px;
        }
        QListWidget#navList::item:hover {
            background-color: #252842; color: #c0c4dc;
        }
        QListWidget#navList::item:selected {
            background-color: #2d3250; color: #7c83fd;
            font-weight: bold; border-left: 3px solid #7c83fd;
        }
        QLabel#versionLabel {
            color: #3d4166; font-size: 10px; padding: 10px;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QStackedWidget#contentArea { background-color: #0f1117; }
        QWidget {
            background-color: #0f1117; color: #c0c4dc;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QGroupBox {
            border: 1px solid #2d3250; border-radius: 8px;
            margin-top: 12px; padding-top: 8px;
            color: #7c83fd; font-weight: bold; font-size: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin; subcontrol-position: top left;
            padding: 0 8px; color: #7c83fd;
        }
        QPushButton {
            background-color: #2d3250; color: #c0c4dc;
            border: 1px solid #3d4466; border-radius: 6px;
            padding: 7px 16px; font-size: 12px; font-weight: bold;
        }
        QPushButton:hover {
            background-color: #3d4466; border-color: #7c83fd; color: #ffffff;
        }
        QPushButton:pressed { background-color: #7c83fd; color: #ffffff; }
        QPushButton:disabled {
            background-color: #1a1d2e; color: #3d4166; border-color: #2d3250;
        }
        QPushButton#primaryBtn {
            background-color: #7c83fd; color: #ffffff; border: none;
        }
        QPushButton#primaryBtn:hover { background-color: #9198ff; }
        QPushButton#dangerBtn {
            background-color: #3d1a1a; color: #ff6b6b; border-color: #ff6b6b;
        }
        QPushButton#dangerBtn:hover { background-color: #ff6b6b; color: #ffffff; }
        QPushButton#successBtn {
            background-color: #1a3d2e; color: #51cf66; border-color: #51cf66;
        }
        QPushButton#successBtn:hover { background-color: #51cf66; color: #ffffff; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #1a1d2e; color: #c0c4dc;
            border: 1px solid #2d3250; border-radius: 6px;
            padding: 6px 10px; font-size: 12px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #7c83fd; }
        QTableWidget {
            background-color: #1a1d2e; color: #c0c4dc;
            border: 1px solid #2d3250; border-radius: 6px;
            gridline-color: #2d3250; font-size: 12px;
        }
        QTableWidget::item { padding: 6px 10px; }
        QTableWidget::item:selected { background-color: #2d3250; color: #7c83fd; }
        QHeaderView::section {
            background-color: #252842; color: #7c83fd;
            border: none; border-bottom: 1px solid #2d3250;
            padding: 8px 10px; font-weight: bold; font-size: 11px;
        }
        QTextEdit {
            background-color: #1a1d2e; color: #a8ff78;
            border: 1px solid #2d3250; border-radius: 6px; padding: 8px;
            font-family: 'Consolas', 'Courier New', monospace; font-size: 11px;
        }
        QScrollBar:vertical {
            background-color: #1a1d2e; width: 8px; border-radius: 4px;
        }
        QScrollBar::handle:vertical { background-color: #2d3250; border-radius: 4px; }
        QScrollBar::handle:vertical:hover { background-color: #7c83fd; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QTabWidget::pane {
            border: 1px solid #2d3250; border-radius: 6px; background-color: #1a1d2e;
        }
        QTabBar::tab {
            background-color: #1a1d2e; color: #8b8fa8;
            padding: 8px 20px; border: 1px solid #2d3250;
            border-bottom: none; border-top-left-radius: 6px;
            border-top-right-radius: 6px; margin-right: 2px;
        }
        QTabBar::tab:selected { background-color: #2d3250; color: #7c83fd; font-weight: bold; }
        QTabBar::tab:hover { background-color: #252842; color: #c0c4dc; }
        QProgressBar {
            background-color: #1a1d2e; border: 1px solid #2d3250;
            border-radius: 4px; text-align: center;
            color: #c0c4dc; font-size: 11px;
        }
        QProgressBar::chunk { background-color: #7c83fd; border-radius: 4px; }
        QLabel#pageTitle { color: #ffffff; font-size: 20px; font-weight: bold; }
        QLabel#pageSubtitle { color: #4a4f6e; font-size: 12px; }
        QLabel#statValue { color: #ffffff; font-size: 28px; font-weight: bold; }
        QLabel#statValueSmall { color: #ffffff; font-size: 18px; font-weight: bold; }
        QLabel#consensusBig { color: #7c83fd; font-size: 48px; font-weight: bold; }
        QLabel#statusOk { color: #51cf66; font-weight: bold; }
        QLabel#statusError { color: #ff6b6b; font-weight: bold; }
        QLabel#statusWarning { color: #ffd43b; font-weight: bold; }
        QSplitter::handle { background-color: #2d3250; width: 2px; }
        QTreeWidget {
            background-color: #1a1d2e; color: #c0c4dc;
            border: 1px solid #2d3250; border-radius: 6px; font-size: 12px;
        }
        QTreeWidget::item:selected { background-color: #2d3250; color: #7c83fd; }
        QScrollArea { border: none; background-color: transparent; }

        /* ── Dialog override ── */
        QDialog { background-color: #1a1d2e; color: #c0c4dc; }
    )");
}
