#include "mainwindow.h"
#include "pages/dashboardpage.h"
#include "pages/devicespage.h"
#include "pages/sensorspage.h"
#include "pages/adminpage.h"
#include "pages/historypage.h"
#include "pages/blockchainexplorerpage.h"
#include "pages/consensusanalysispage.h"
#include <QListWidgetItem>
#include <QJsonDocument>


TxFeedbackDialog::TxFeedbackDialog(const QString &operation,
                                   const QJsonObject &txData,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Transaction Result  " + operation);
    setMinimumSize(600, 380);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    bool ok = txData["status"].toString() == "success";
    auto *statusLabel = new QLabel(ok ? " Transaction Confirmed" : " Transaction Failed");
    statusLabel->setStyleSheet(
        QString("font-size: 16px; font-weight: bold; color: %1;")
            .arg(ok ? "#2b8c4e" : "#c92a2a")
        );
    layout->addWidget(statusLabel);

    auto addRow = [&](const QString &label, const QString &value, bool mono = false) {
        auto *row   = new QHBoxLayout();
        auto *lbl   = new QLabel(label + ":");
        lbl->setFixedWidth(160);
        lbl->setStyleSheet("color: #6c757d; font-size: 12px;");
        auto *val   = new QLabel(value);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        val->setWordWrap(true);
        val->setStyleSheet(
            mono ? "color: #4c6ef5; font-family: Consolas, monospace; font-size: 11px;"
                 : "color: #212529; font-size: 12px;"
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

    auto *hint = new QLabel("Copy the Tx Hash and paste it into the Blockchain Explorer tab to inspect decoded logs.");
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #6c757d; font-size: 11px; font-style: italic;");
    layout->addWidget(hint);

    auto *closeBtn = new QPushButton("Close");
    closeBtn->setObjectName("primaryBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    setStyleSheet(R"(
        QDialog { background-color: #ffffff; }
        QPushButton#primaryBtn {
            background-color: #4c6ef5; color: #ffffff;
            border: none; border-radius: 6px;
            padding: 7px 20px; font-weight: bold;
        }
        QPushButton#primaryBtn:hover { background-color: #3b5bdb; }
    )");
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_api(new ApiClient(this))
{
    setWindowTitle("SensorChain  IoT Blockchain Dashboard");
    setMinimumSize(1200, 750);
    resize(1440, 880);

    setupUi();
    setupStyles();
    setupGlobalConnections();

    m_navList->setCurrentRow(0);
}

MainWindow::~MainWindow() {}


void MainWindow::setupUi()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_mainLayout = new QHBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    //  Side navigation panel 
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

    m_logoLabel = new QLabel("SensorChain");
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
    m_navList->setFocusPolicy(Qt::NoFocus);

    addNavItem("", "Dashboard");
    addNavItem("", "Devices");
    addNavItem("", "Sensors");
    addNavItem("",  "Admin");
    addNavItem("", "History");
    addNavItem("",  "Blockchain");    // NEW  index 5
    addNavItem("", "Analysis");      // NEW  index 6

    m_navLayout->addWidget(m_navList, 1);

    m_versionLabel = new QLabel("v1.1.0  |  Ganache Local");
    m_versionLabel->setObjectName("versionLabel");
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_navLayout->addWidget(m_versionLabel);

    //  Content pages 
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


void MainWindow::setupGlobalConnections()
{
    // Every write operation  show TxFeedbackDialog
    connect(m_api, &ApiClient::deviceRegistered,
            this,  &MainWindow::onDeviceRegistered);
    connect(m_api, &ApiClient::firmwareUpdated,
            this,  &MainWindow::onFirmwareUpdated);
    connect(m_api, &ApiClient::deviceDeactivated,
            this,  &MainWindow::onDeviceDeactivated);
    connect(m_api, &ApiClient::deviceReactivated,
            this,  &MainWindow::onDeviceReactivated);
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

// Write result slots  each calls showTxFeedback

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


void MainWindow::setupStyles()
{
    setStyleSheet(R"(
        QMainWindow { background-color: #f8f9fa; }

        QWidget#navPanel {
            background-color: #ffffff;
            border-right: 1px solid #e9ecef;
        }
        QWidget#logoArea { background-color: #ffffff; padding: 10px; }
        QLabel#logoLabel {
            color: #4c6ef5; font-size: 16px;
            font-weight: bold; font-family: 'Segoe UI', Arial, sans-serif;
        }
        QLabel#logoSubtitle {
            color: #868e96; font-size: 10px;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QFrame#navSeparator {
            color: #e9ecef; background-color: #e9ecef;
            border: none; height: 1px;
        }
        QListWidget#navList {
            background-color: #ffffff; color: #495057;
            font-size: 13px; font-family: 'Segoe UI', Arial, sans-serif;
            border: none; padding: 8px 0px;
        }
        QListWidget#navList::item {
            padding: 8px 20px; margin: 2px 8px; border-radius: 6px;
        }
        QListWidget#navList::item:hover {
            background-color: #f1f3f5; color: #212529;
        }
        QListWidget#navList::item:selected {
            background-color: #e7f5ff; color: #4c6ef5;
            font-weight: bold; border-left: 3px solid #4c6ef5;
        }
            QListWidget#navList::item:focus {
                outline: none;
            }
        QLabel#versionLabel {
            color: #adb5bd; font-size: 10px; padding: 10px;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QStackedWidget#contentArea { background-color: #f8f9fa; }
        QWidget {
            color: #212529;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QGroupBox {
            border: 1px solid #dee2e6; border-radius: 8px;
            margin-top: 12px; padding-top: 8px;
            color: #4c6ef5; font-weight: bold; font-size: 12px;
            background-color: #ffffff;
        }
        QGroupBox::title {
            subcontrol-origin: margin; subcontrol-position: top left;
            padding: 0 8px; color: #4c6ef5;
        }
        QPushButton {
            background-color: #ffffff; color: #495057;
            border: 1px solid #dee2e6; border-radius: 6px;
            padding: 7px 16px; font-size: 12px; font-weight: bold;
        }
        QPushButton:hover {
            background-color: #f8f9fa; border-color: #4c6ef5; color: #4c6ef5;
        }
        QPushButton:pressed { background-color: #4c6ef5; color: #ffffff; }
        QPushButton:disabled {
            background-color: #e9ecef; color: #adb5bd; border-color: #dee2e6;
        }
        QPushButton#primaryBtn {
            background-color: #4c6ef5; color: #ffffff; border: none;
        }
        QPushButton#primaryBtn:hover { background-color: #3b5bdb; }
        QPushButton#dangerBtn {
            background-color: #fff5f5; color: #c92a2a; border-color: #c92a2a;
        }
        QPushButton#dangerBtn:hover { background-color: #c92a2a; color: #ffffff; }
        QPushButton#successBtn {
            background-color: #ebfbee; color: #2b8c4e; border-color: #2b8c4e;
        }
        QPushButton#successBtn:hover { background-color: #2b8c4e; color: #ffffff; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #ffffff; color: #212529;
            border: 1px solid #dee2e6; border-radius: 6px;
            padding: 6px 10px; font-size: 12px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #4c6ef5; }
        QTableWidget {
            background-color: #ffffff; color: #212529;
            border: 1px solid #dee2e6; border-radius: 6px;
            gridline-color: #f1f3f5; font-size: 12px;
        }
        QTableWidget::item { padding: 6px 10px; }
        QTableWidget::item:selected { background-color: #e7f5ff; color: #4c6ef5; }
        QHeaderView::section {
            background-color: #f8f9fa; color: #495057;
            border: none; border-bottom: 1px solid #dee2e6;
            padding: 8px 10px; font-weight: bold; font-size: 11px;
        }
        QTextEdit {
            background-color: #f8f9fa; color: #212529;
            border: 1px solid #dee2e6; border-radius: 6px; padding: 8px;
            font-family: 'Consolas', 'Courier New', monospace; font-size: 11px;
        }
        QScrollBar:vertical {
            background-color: #f8f9fa; width: 8px; border-radius: 4px;
        }
        QScrollBar::handle:vertical { background-color: #dee2e6; border-radius: 4px; }
        QScrollBar::handle:vertical:hover { background-color: #4c6ef5; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QTabWidget::pane {
            border: 1px solid #dee2e6; border-radius: 6px; background-color: #ffffff;
        }
        QTabBar::tab {
            background-color: #f8f9fa; color: #868e96;
            padding: 8px 20px; border: 1px solid #dee2e6;
            border-bottom: none; border-top-left-radius: 6px;
            border-top-right-radius: 6px; margin-right: 2px;
        }
        QTabBar::tab:selected { background-color: #ffffff; color: #4c6ef5; font-weight: bold; }
        QTabBar::tab:hover { background-color: #e9ecef; color: #212529; }
        QProgressBar {
            background-color: #e9ecef; border: 1px solid #dee2e6;
            border-radius: 4px; text-align: center;
            color: #495057; font-size: 11px;
        }
        QProgressBar::chunk { background-color: #4c6ef5; border-radius: 4px; }
        QLabel#pageTitle { color: #212529; font-size: 20px; font-weight: bold; }
        QLabel#pageSubtitle { color: #868e96; font-size: 12px; }
        QLabel#statValue { color: #212529; font-size: 28px; font-weight: bold; }
        QLabel#statValueSmall { color: #212529; font-size: 18px; font-weight: bold; }
        QLabel#consensusBig { color: #4c6ef5; font-size: 48px; font-weight: bold; }
        QLabel#statusOk { color: #2b8c4e; font-weight: bold; }
        QLabel#statusError { color: #c92a2a; font-weight: bold; }
        QLabel#statusWarning { color: #e67700; font-weight: bold; }
        QSplitter::handle { background-color: #dee2e6; width: 2px; }
        QTreeWidget {
            background-color: #ffffff; color: #212529;
            border: 1px solid #dee2e6; border-radius: 6px; font-size: 12px;
        }
        QTreeWidget::item:selected { background-color: #e7f5ff; color: #4c6ef5; }
        QScrollArea { border: none; background-color: transparent; }

        /*  Dialog override  */
        QDialog { background-color: #ffffff; color: #212529; }
    )");
}
