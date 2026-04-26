#include "devicespage.h"
#include <QHeaderView>
#include <QDateTime>

DevicesPage::DevicesPage(ApiClient *api, QWidget *parent)
    : QWidget(parent), m_api(api)
{
    setupUi();

    connect(m_api, &ApiClient::allDevicesReady,       this, &DevicesPage::onAllDevices);
    connect(m_api, &ApiClient::deviceInfoReady,       this, &DevicesPage::onDeviceInfo);
    connect(m_api, &ApiClient::verifyDeviceReady,     this, &DevicesPage::onVerifyDevice);
    connect(m_api, &ApiClient::isDeviceRegisteredReady, this, &DevicesPage::onIsRegistered);
    connect(m_api, &ApiClient::deviceFirmwareReady,   this, &DevicesPage::onDeviceFirmware);
    connect(m_api, &ApiClient::verifyFirmwareReady,   this, &DevicesPage::onVerifyFirmware);
    connect(m_api, &ApiClient::requestError,          this, &DevicesPage::onError);

    refresh();
}

void DevicesPage::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel *title = new QLabel("Devices");
    title->setObjectName("pageTitle");
    QLabel *subtitle = new QLabel("Registered IoT devices and firmware verification");
    subtitle->setObjectName("pageSubtitle");
    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    // Splitter: table left, query panel right
    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    //  Left: all devices table 
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0,0,0,0);

    QHBoxLayout *tableHeader = new QHBoxLayout();
    QLabel *tableTitle = new QLabel("All Registered Devices");
    tableTitle->setObjectName("pageSubtitle");
    m_refreshBtn = new QPushButton("Refresh");
    tableHeader->addWidget(tableTitle);
    tableHeader->addStretch();
    tableHeader->addWidget(m_refreshBtn);
    leftLayout->addLayout(tableHeader);

    m_devicesTable = new QTableWidget(0, 5);
    m_devicesTable->setHorizontalHeaderLabels({"Address", "Type", "Version", "Active", "Registered"});
    m_devicesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_devicesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_devicesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_devicesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_devicesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_devicesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_devicesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_devicesTable->setFocusPolicy(Qt::NoFocus);
    leftLayout->addWidget(m_devicesTable);

    //  Right: query panel 
    QWidget *rightWidget = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(12);

    QGroupBox *queryGroup = new QGroupBox("Query Device");
    QVBoxLayout *queryLayout = new QVBoxLayout(queryGroup);

    QLabel *addrLbl = new QLabel("Device Address:");
    m_addressInput = new QLineEdit();
    m_addressInput->setPlaceholderText("0x...");

    QHBoxLayout *btnRow1 = new QHBoxLayout();
    m_getInfoBtn       = new QPushButton("Get Info");
    m_verifyBtn        = new QPushButton("Verify Active");
    m_isRegisteredBtn  = new QPushButton("Is Registered?");
    m_getFirmwareBtn   = new QPushButton("Get Firmware");
    m_getInfoBtn->setObjectName("primaryBtn");
    btnRow1->addWidget(m_getInfoBtn);
    btnRow1->addWidget(m_verifyBtn);

    QHBoxLayout *btnRow2 = new QHBoxLayout();
    btnRow2->addWidget(m_isRegisteredBtn);
    btnRow2->addWidget(m_getFirmwareBtn);

    queryLayout->addWidget(addrLbl);
    queryLayout->addWidget(m_addressInput);
    queryLayout->addLayout(btnRow1);
    queryLayout->addLayout(btnRow2);

    QGroupBox *firmwareGroup = new QGroupBox("Verify Firmware Hash");
    QVBoxLayout *firmwareLayout = new QVBoxLayout(firmwareGroup);
    QLabel *hashLbl = new QLabel("Firmware Hash (bytes32):");
    m_firmwareHashInput = new QLineEdit();
    m_firmwareHashInput->setPlaceholderText("0x1234...");
    m_verifyFirmwareBtn = new QPushButton("Verify Firmware");
    firmwareLayout->addWidget(hashLbl);
    firmwareLayout->addWidget(m_firmwareHashInput);
    firmwareLayout->addWidget(m_verifyFirmwareBtn);

    QGroupBox *resultGroup = new QGroupBox("Result");
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    m_detailDisplay = new QTextEdit();
    m_detailDisplay->setReadOnly(true);
    m_detailDisplay->setMinimumHeight(220);
    resultLayout->addWidget(m_detailDisplay);

    rightLayout->addWidget(queryGroup);
    rightLayout->addWidget(firmwareGroup);
    rightLayout->addWidget(resultGroup, 1);
    rightLayout->addStretch();

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    mainLayout->addWidget(splitter, 1);

    connect(m_refreshBtn,       &QPushButton::clicked, this, &DevicesPage::refresh);
    connect(m_getInfoBtn,       &QPushButton::clicked, this, &DevicesPage::onGetInfoClicked);
    connect(m_verifyBtn,        &QPushButton::clicked, this, &DevicesPage::onVerifyClicked);
    connect(m_isRegisteredBtn,  &QPushButton::clicked, this, &DevicesPage::onIsRegisteredClicked);
    connect(m_getFirmwareBtn,   &QPushButton::clicked, this, &DevicesPage::onGetFirmwareClicked);
    connect(m_verifyFirmwareBtn,&QPushButton::clicked, this, &DevicesPage::onVerifyFirmwareClicked);
    connect(m_devicesTable, &QTableWidget::cellClicked, this, &DevicesPage::onDeviceRowClicked);
}

void DevicesPage::refresh()
{
    m_api->getAllDevices();
}

void DevicesPage::onAllDevices(QJsonArray devices)
{
    populateTable(devices);
}

void DevicesPage::populateTable(const QJsonArray &devices)
{
    m_devicesTable->setRowCount(0);
    for (int i = 0; i < devices.size(); i++) {
        QJsonObject d = devices[i].toObject();
        m_devicesTable->insertRow(i);

        auto item = [](const QString &text) {
            return new QTableWidgetItem(text);
        };

        m_devicesTable->setItem(i, 0, item(d["deviceAddress"].toString()));
        m_devicesTable->setItem(i, 1, item(d["deviceType"].toString()));
        m_devicesTable->setItem(i, 2, item("v" + d["firmwareVersion"].toString()));

        bool active = d["isActive"].toBool();
        QTableWidgetItem *activeItem = item(active ? "Active" : "Inactive");
        activeItem->setForeground(active ? QColor("#2b8c4e") : QColor("#c92a2a"));
        m_devicesTable->setItem(i, 3, activeItem);

        QString ts = d["registrationTime"].toString("0");
        QDateTime dt = QDateTime::fromSecsSinceEpoch(ts.toLongLong());
        m_devicesTable->setItem(i, 4, item(dt.toString("dd/MM/yyyy")));
    }
}

void DevicesPage::onDeviceRowClicked(int row, int col)
{
    Q_UNUSED(col)
    QString address = m_devicesTable->item(row, 0)->text();
    m_addressInput->setText(address);
    m_api->getDeviceInfo(address);
}

void DevicesPage::onDeviceInfo(QJsonObject device)
{
    showDeviceDetail(device);
}

void DevicesPage::showDeviceDetail(const QJsonObject &d)
{
    QString ts1 = d["registrationTime"].toString("0");
    QString ts2 = d["lastUpdate"].toString("0");
    QDateTime reg = QDateTime::fromSecsSinceEpoch(ts1.toLongLong());
    QDateTime upd = QDateTime::fromSecsSinceEpoch(ts2.toLongLong());

    QString text;
    text += "=== Device Info ===\n";
    text += "Address:       " + d["deviceAddress"].toString() + "\n";
    text += "Type:          " + d["deviceType"].toString() + "\n";
    text += "Firmware v:    " + d["firmwareVersion"].toString() + "\n";
    text += "Firmware Hash: " + d["firmwareHash"].toString() + "\n";
    text += "Active:        " + QString(d["isActive"].toBool() ? "YES" : "NO") + "\n";
    text += "Registered:    " + reg.toString("dd/MM/yyyy hh:mm:ss") + "\n";
    text += "Last Update:   " + upd.toString("dd/MM/yyyy hh:mm:ss") + "\n";
    m_detailDisplay->setText(text);
}

void DevicesPage::onVerifyDevice(QString address, bool isActive)
{
    m_detailDisplay->setText(
        "=== Verify Device ===\n"
        "Address: " + address + "\n"
                    "Result:  " + QString(isActive ? "Registered and ACTIVE" : "NOT active or not registered")
        );
}

void DevicesPage::onIsRegistered(QString address, bool isRegistered)
{
    m_detailDisplay->setText(
        "=== Is Registered ===\n"
        "Address: " + address + "\n"
                    "Result:  " + QString(isRegistered ? "YES  device has been registered" : "NO  never registered")
        );
}

void DevicesPage::onDeviceFirmware(QJsonObject firmware)
{
    QString ts = firmware["lastUpdate"].toString("0");
    QDateTime dt = QDateTime::fromSecsSinceEpoch(ts.toLongLong());
    m_detailDisplay->setText(
        "=== Firmware Info ===\n"
        "Hash:        " + firmware["firmwareHash"].toString() + "\n"
                                                "Version:     " + firmware["firmwareVersion"].toString() + "\n"
                                                   "Last Update: " + dt.toString("dd/MM/yyyy hh:mm:ss")
        );
}

void DevicesPage::onVerifyFirmware(QString address, bool matches)
{
    m_detailDisplay->setText(
        "=== Verify Firmware ===\n"
        "Address: " + address + "\n"
                    "Hash:    " + m_firmwareHashInput->text() + "\n"
                                        "Result:  " + QString(matches ? "Hash MATCHES on-chain record" : "Hash does NOT match")
        );
}

void DevicesPage::onError(QString endpoint, QString error)
{
    m_detailDisplay->setText("Error on " + endpoint + "\n" + error);
}

void DevicesPage::onGetInfoClicked()
{
    QString addr = m_addressInput->text().trimmed();
    if (!addr.isEmpty()) m_api->getDeviceInfo(addr);
}

void DevicesPage::onVerifyClicked()
{
    QString addr = m_addressInput->text().trimmed();
    if (!addr.isEmpty()) m_api->verifyDevice(addr);
}

void DevicesPage::onIsRegisteredClicked()
{
    QString addr = m_addressInput->text().trimmed();
    if (!addr.isEmpty()) m_api->isDeviceRegistered(addr);
}

void DevicesPage::onGetFirmwareClicked()
{
    QString addr = m_addressInput->text().trimmed();
    if (!addr.isEmpty()) m_api->getDeviceFirmware(addr);
}

void DevicesPage::onVerifyFirmwareClicked()
{
    QString addr = m_addressInput->text().trimmed();
    QString hash = m_firmwareHashInput->text().trimmed();
    if (!addr.isEmpty() && !hash.isEmpty()) m_api->verifyFirmware(addr, hash);
}
