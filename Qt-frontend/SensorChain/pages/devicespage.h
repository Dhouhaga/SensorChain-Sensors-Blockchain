#ifndef DEVICESPAGE_H
#define DEVICESPAGE_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QTextEdit>
#include "../apiclient.h"

class DevicesPage : public QWidget
{
    Q_OBJECT

public:
    explicit DevicesPage(ApiClient *api, QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void onAllDevices(QJsonArray devices);
    void onDeviceInfo(QJsonObject device);
    void onVerifyDevice(QString address, bool isActive);
    void onIsRegistered(QString address, bool isRegistered);
    void onDeviceFirmware(QJsonObject firmware);
    void onVerifyFirmware(QString address, bool matches);
    void onError(QString endpoint, QString error);
    void onDeviceRowClicked(int row, int col);
    void onVerifyClicked();
    void onIsRegisteredClicked();
    void onVerifyFirmwareClicked();
    void onGetFirmwareClicked();
    void onGetInfoClicked();

private:
    void setupUi();
    void populateTable(const QJsonArray &devices);
    void showDeviceDetail(const QJsonObject &device);

    ApiClient *m_api;

    // Left: devices table
    QTableWidget *m_devicesTable;
    QPushButton  *m_refreshBtn;

    // Right: query panel
    QLineEdit    *m_addressInput;
    QPushButton  *m_getInfoBtn;
    QPushButton  *m_verifyBtn;
    QPushButton  *m_isRegisteredBtn;
    QPushButton  *m_getFirmwareBtn;

    QLineEdit    *m_firmwareHashInput;
    QPushButton  *m_verifyFirmwareBtn;

    QTextEdit    *m_detailDisplay;
};

#endif 
