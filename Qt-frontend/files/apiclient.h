#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

/**
 * ApiClient — single class that handles all HTTP communication with the backend.
 * Every page in the app uses this class. No raw QNetworkAccessManager elsewhere.
 *
 * Usage:
 *   ApiClient *api = new ApiClient(this);
 *   connect(api, &ApiClient::registryStatsReady, this, &MyPage::onStats);
 *   api->getRegistryStats();
 */
class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    // Base URL — change if backend runs on a different port
    static const QString BASE_URL;

    // ================================================================
    // DEVICE REGISTRY — READ
    // ================================================================
    void getRegistryStats();
    void getAllDevices();
    void getDeviceInfo(const QString &address);
    void verifyDevice(const QString &address);
    void isDeviceRegistered(const QString &address);
    void getDeviceFirmware(const QString &address);
    void verifyFirmware(const QString &address, const QString &hash);

    // ================================================================
    // DEVICE REGISTRY — WRITE (admin)
    // ================================================================
    void registerDevice(const QString &deviceAddress,
                        const QString &firmwareHash,
                        int firmwareVersion,
                        const QString &deviceType);
    void updateFirmware(const QString &deviceAddress,
                        const QString &newFirmwareHash,
                        int newVersion);
    void deactivateDevice(const QString &deviceAddress);
    void reactivateDevice(const QString &deviceAddress);

    // ================================================================
    // SENSOR CONSENSUS — READ
    // ================================================================
    void getConsensusStats();
    void getLatestConsensus();
    void getAllRounds();
    void getCurrentRound();
    void getConsensusRound(int roundId);
    void getReading(int roundId, const QString &sensorAddress);
    void getEventHistory(int fromBlock = 0);

    // ================================================================
    // SENSOR CONSENSUS — WRITE (sensor)
    // ================================================================
    void submitReading(const QString &sensorAddress,
                       int value,
                       const QString &firmwareHash);

    // ================================================================
    // SENSOR CONSENSUS — WRITE (admin)
    // ================================================================
    void forceNewRound();
    void forceConsensus();
    void setFaultyThreshold(int threshold);
    void setMinSensors(int minSensors);
    void setConsensusWindow(int windowSeconds);
    void setDeviceRegistry(const QString &registryAddress);

signals:
    // ── Registry read signals ────────────────────────────────────────
    void registryStatsReady(QJsonObject data);
    void allDevicesReady(QJsonArray devices);
    void deviceInfoReady(QJsonObject device);
    void verifyDeviceReady(QString address, bool isActive);
    void isDeviceRegisteredReady(QString address, bool isRegistered);
    void deviceFirmwareReady(QJsonObject firmware);
    void verifyFirmwareReady(QString address, bool matches);

    // ── Registry write signals ───────────────────────────────────────
    void deviceRegistered(QJsonObject receipt);
    void firmwareUpdated(QJsonObject receipt);
    void deviceDeactivated(QJsonObject receipt);
    void deviceReactivated(QJsonObject receipt);

    // ── Consensus read signals ───────────────────────────────────────
    void consensusStatsReady(QJsonObject data);
    void latestConsensusReady(QJsonObject data);
    void allRoundsReady(QJsonArray rounds);
    void currentRoundReady(QJsonObject data);
    void consensusRoundReady(QJsonObject round);
    void readingReady(QJsonObject reading);
    void eventHistoryReady(QJsonObject events);

    // ── Consensus write signals ──────────────────────────────────────
    void readingSubmitted(QJsonObject receipt);
    void newRoundStarted(QJsonObject receipt);
    void consensusForced(QJsonObject receipt);
    void faultyThresholdSet(QJsonObject receipt);
    void minSensorsSet(QJsonObject receipt);
    void consensusWindowSet(QJsonObject receipt);
    void deviceRegistrySet(QJsonObject receipt);

    // ── Error signal (any request) ───────────────────────────────────
    void requestError(QString endpoint, QString errorMessage);

private:
    QNetworkAccessManager *m_manager;

    // HTTP helpers
    void get(const QString &endpoint,
             std::function<void(QJsonObject)> handler);

    void post(const QString &endpoint,
              const QJsonObject &body,
              std::function<void(QJsonObject)> handler);

    void handleReply(QNetworkReply *reply,
                     const QString &endpoint,
                     std::function<void(QJsonObject)> handler);
};

#endif // APICLIENT_H
