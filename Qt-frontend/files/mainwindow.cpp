#include "mainwindow.h"
#include "pages/dashboardpage.h"
#include "pages/devicespage.h"
#include "pages/sensorspage.h"
#include "pages/adminpage.h"
#include "pages/historypage.h"
#include <QListWidgetItem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_api(new ApiClient(this))
{
    setWindowTitle("SensorChain — IoT Blockchain Dashboard");
    setMinimumSize(1200, 750);
    resize(1400, 850);

    setupUi();
    setupStyles();

    // Start on dashboard
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

    // ── Side navigation panel ────────────────────────────────────────
    m_navPanel = new QWidget();
    m_navPanel->setFixedWidth(220);
    m_navPanel->setObjectName("navPanel");

    m_navLayout = new QVBoxLayout(m_navPanel);
    m_navLayout->setContentsMargins(0, 0, 0, 0);
    m_navLayout->setSpacing(0);

    // Logo / title area
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

    // Separator
    QFrame *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("navSeparator");
    m_navLayout->addWidget(sep);

    // Navigation list
    m_navList = new QListWidget();
    m_navList->setObjectName("navList");
    m_navList->setFrameShape(QFrame::NoFrame);

    addNavItem("📊", "Dashboard");
    addNavItem("🔌", "Devices");
    addNavItem("📡", "Sensors");
    addNavItem("⚙️", "Admin");
    addNavItem("📜", "History");

    m_navLayout->addWidget(m_navList, 1);

    // Version label at bottom
    m_versionLabel = new QLabel("v1.0.0  |  Ganache Local");
    m_versionLabel->setObjectName("versionLabel");
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_navLayout->addWidget(m_versionLabel);

    // ── Content pages ────────────────────────────────────────────────
    m_pages = new QStackedWidget();
    m_pages->setObjectName("contentArea");

    m_dashboardPage = new DashboardPage(m_api, this);
    m_devicesPage   = new DevicesPage(m_api, this);
    m_sensorsPage   = new SensorsPage(m_api, this);
    m_adminPage     = new AdminPage(m_api, this);
    m_historyPage   = new HistoryPage(m_api, this);

    m_pages->addWidget(m_dashboardPage);  // index 0
    m_pages->addWidget(m_devicesPage);    // index 1
    m_pages->addWidget(m_sensorsPage);    // index 2
    m_pages->addWidget(m_adminPage);      // index 3
    m_pages->addWidget(m_historyPage);    // index 4

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

void MainWindow::onNavItemChanged(int index)
{
    m_pages->setCurrentIndex(index);

    // Refresh page data when switching to it
    switch (index) {
    case 0: m_dashboardPage->refresh(); break;
    case 1: m_devicesPage->refresh();   break;
    case 2: m_sensorsPage->refresh();   break;
    case 3: m_adminPage->refresh();     break;
    case 4: m_historyPage->refresh();   break;
    }
}

void MainWindow::setupStyles()
{
    setStyleSheet(R"(
        /* ── Main window ── */
        QMainWindow {
            background-color: #0f1117;
        }

        /* ── Side nav panel ── */
        QWidget#navPanel {
            background-color: #1a1d2e;
            border-right: 1px solid #2d3250;
        }

        QWidget#logoArea {
            background-color: #1a1d2e;
            padding: 10px;
        }

        QLabel#logoLabel {
            color: #7c83fd;
            font-size: 16px;
            font-weight: bold;
            font-family: 'Segoe UI', Arial, sans-serif;
        }

        QLabel#logoSubtitle {
            color: #4a4f6e;
            font-size: 10px;
            font-family: 'Segoe UI', Arial, sans-serif;
        }

        QFrame#navSeparator {
            color: #2d3250;
            background-color: #2d3250;
            border: none;
            height: 1px;
        }

        QListWidget#navList {
            background-color: #1a1d2e;
            color: #8b8fa8;
            font-size: 13px;
            font-family: 'Segoe UI', Arial, sans-serif;
            border: none;
            padding: 8px 0px;
        }

        QListWidget#navList::item {
            padding: 8px 20px;
            border-radius: 0px;
            margin: 2px 8px;
            border-radius: 6px;
        }

        QListWidget#navList::item:hover {
            background-color: #252842;
            color: #c0c4dc;
        }

        QListWidget#navList::item:selected {
            background-color: #2d3250;
            color: #7c83fd;
            font-weight: bold;
            border-left: 3px solid #7c83fd;
        }

        QLabel#versionLabel {
            color: #3d4166;
            font-size: 10px;
            padding: 10px;
            font-family: 'Segoe UI', Arial, sans-serif;
        }

        /* ── Content area ── */
        QStackedWidget#contentArea {
            background-color: #0f1117;
        }

        /* ── General widgets ── */
        QWidget {
            background-color: #0f1117;
            color: #c0c4dc;
            font-family: 'Segoe UI', Arial, sans-serif;
        }

        QGroupBox {
            border: 1px solid #2d3250;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 8px;
            color: #7c83fd;
            font-weight: bold;
            font-size: 12px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            color: #7c83fd;
        }

        QPushButton {
            background-color: #2d3250;
            color: #c0c4dc;
            border: 1px solid #3d4466;
            border-radius: 6px;
            padding: 7px 16px;
            font-size: 12px;
            font-weight: bold;
        }

        QPushButton:hover {
            background-color: #3d4466;
            border-color: #7c83fd;
            color: #ffffff;
        }

        QPushButton:pressed {
            background-color: #7c83fd;
            color: #ffffff;
        }

        QPushButton:disabled {
            background-color: #1a1d2e;
            color: #3d4166;
            border-color: #2d3250;
        }

        QPushButton#primaryBtn {
            background-color: #7c83fd;
            color: #ffffff;
            border: none;
        }

        QPushButton#primaryBtn:hover {
            background-color: #9198ff;
        }

        QPushButton#dangerBtn {
            background-color: #3d1a1a;
            color: #ff6b6b;
            border-color: #ff6b6b;
        }

        QPushButton#dangerBtn:hover {
            background-color: #ff6b6b;
            color: #ffffff;
        }

        QPushButton#successBtn {
            background-color: #1a3d2e;
            color: #51cf66;
            border-color: #51cf66;
        }

        QPushButton#successBtn:hover {
            background-color: #51cf66;
            color: #ffffff;
        }

        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #1a1d2e;
            color: #c0c4dc;
            border: 1px solid #2d3250;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }

        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #7c83fd;
        }

        QTableWidget {
            background-color: #1a1d2e;
            color: #c0c4dc;
            border: 1px solid #2d3250;
            border-radius: 6px;
            gridline-color: #2d3250;
            font-size: 12px;
        }

        QTableWidget::item {
            padding: 6px 10px;
        }

        QTableWidget::item:selected {
            background-color: #2d3250;
            color: #7c83fd;
        }

        QHeaderView::section {
            background-color: #252842;
            color: #7c83fd;
            border: none;
            border-bottom: 1px solid #2d3250;
            padding: 8px 10px;
            font-weight: bold;
            font-size: 11px;
        }

        QTextEdit {
            background-color: #1a1d2e;
            color: #a8ff78;
            border: 1px solid #2d3250;
            border-radius: 6px;
            padding: 8px;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 11px;
        }

        QScrollBar:vertical {
            background-color: #1a1d2e;
            width: 8px;
            border-radius: 4px;
        }

        QScrollBar::handle:vertical {
            background-color: #2d3250;
            border-radius: 4px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #7c83fd;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QTabWidget::pane {
            border: 1px solid #2d3250;
            border-radius: 6px;
            background-color: #1a1d2e;
        }

        QTabBar::tab {
            background-color: #1a1d2e;
            color: #8b8fa8;
            padding: 8px 20px;
            border: 1px solid #2d3250;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            margin-right: 2px;
        }

        QTabBar::tab:selected {
            background-color: #2d3250;
            color: #7c83fd;
            font-weight: bold;
        }

        QTabBar::tab:hover {
            background-color: #252842;
            color: #c0c4dc;
        }

        QProgressBar {
            background-color: #1a1d2e;
            border: 1px solid #2d3250;
            border-radius: 4px;
            text-align: center;
            color: #c0c4dc;
            font-size: 11px;
        }

        QProgressBar::chunk {
            background-color: #7c83fd;
            border-radius: 4px;
        }

        QLabel#pageTitle {
            color: #ffffff;
            font-size: 20px;
            font-weight: bold;
            padding: 0px 0px 4px 0px;
        }

        QLabel#pageSubtitle {
            color: #4a4f6e;
            font-size: 12px;
        }

        QLabel#statValue {
            color: #ffffff;
            font-size: 28px;
            font-weight: bold;
        }

        QLabel#statValueSmall {
            color: #ffffff;
            font-size: 18px;
            font-weight: bold;
        }

        QLabel#consensusBig {
            color: #7c83fd;
            font-size: 48px;
            font-weight: bold;
        }

        QLabel#statusOk {
            color: #51cf66;
            font-weight: bold;
        }

        QLabel#statusError {
            color: #ff6b6b;
            font-weight: bold;
        }

        QLabel#statusWarning {
            color: #ffd43b;
            font-weight: bold;
        }

        QSplitter::handle {
            background-color: #2d3250;
            width: 2px;
        }

        QTreeWidget {
            background-color: #1a1d2e;
            color: #c0c4dc;
            border: 1px solid #2d3250;
            border-radius: 6px;
            font-size: 12px;
        }

        QTreeWidget::item:selected {
            background-color: #2d3250;
            color: #7c83fd;
        }

        QScrollArea {
            border: none;
            background-color: transparent;
        }
    )");
}
