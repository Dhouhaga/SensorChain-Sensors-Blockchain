#ifndef SENSORSPAGE_H
#define SENSORSPAGE_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QTextEdit>
#include <QSlider>
#include "../apiclient.h"

class SensorsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SensorsPage(ApiClient *api, QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void onAllDevices(QJsonArray devices);
    void onReadingSubmitted(QJsonObject receipt);
    void onCurrentRound(QJsonObject data);
    void onReadingReady(QJsonObject reading);
    void onError(QString endpoint, QString error);
    void onSubmitClicked();
    void onSensorSelected(int index);
    void onSliderChanged(int value);
    void onSpinChanged(int value);
    void onLookupReadingClicked();

private:
    void setupUi();
    void populateSensorDropdown(const QJsonArray &devices);
    void setStatus(const QString &msg, bool success);

    ApiClient *m_api;

    QComboBox    *m_sensorDropdown;
    QLabel       *m_sensorAddressLabel;
    QLabel       *m_sensorFirmwareLabel;
    QSlider      *m_valueSlider;
    QSpinBox     *m_valueSpinBox;
    QLabel       *m_valueTempLabel;
    QLineEdit    *m_firmwareHashInput;
    QPushButton  *m_submitBtn;
    QLabel       *m_statusLabel;

    QTextEdit    *m_resultDisplay;

    QSpinBox     *m_lookupRoundSpinBox;
    QLineEdit    *m_lookupSensorInput;
    QPushButton  *m_lookupBtn;
    QTextEdit    *m_lookupDisplay;

    QTableWidget *m_participantsTable;

    // Sensor address  firmware hash map (populated from getAllDevices)
    QMap<QString, QString> m_sensorFirmwareMap;
    QMap<QString, bool> m_sensorActiveMap;

    void showTransactionFailureDialog(const QString &txHash, const QString &reason, const QString &errorMsg);
    void showTransactionSuccessDialog(const QString &txHash, int blockNumber, const QString &gasUsed,
                                      const QString &sensorAddress, const QString &value);
};

#endif // SENSORSPAGE_H
