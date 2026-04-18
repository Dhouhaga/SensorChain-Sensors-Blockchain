#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <functional>

/**
 * @class ApiClient
 *
 * Thin HTTP wrapper around the IoT Blockchain backend.
 *
 * ENHANCED (v1.1):
 *   • All write-signal payloads now carry enriched transaction data
 *     (txHash, blockNumber, blockHash, from, to, gasUsed, gasPrice,
 *      status, confirmations, blockTimestamp, logsCount).
 *   • New blockchain inspector signals  (blockchainLatestReady,
 *     blockchainBlockReady, blockchainTxReady, blockchainNetworkReady).
 *   • New consensus explanation signal  (consensusExplainReady).
 */
class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    // ── DeviceRegistry — read ────────────────────────────────────
    void getRegistryStats();
    void getAllDevices();
    void getDeviceInfo(const QString &address);
    void verifyDevice(const QString &address);
    void isDeviceRegistered(const QString &address);
    void getDeviceFirmware(const QString &address);
    void verifyFirmware(const QString &address, const QString &hash);

    // ── DeviceRegistry — write ───────────────────────────────────
    void registerDevice(const QString &deviceAddress,
                        const QString &firmwareHash,
                        int firmwareVersion,
                        const QString &deviceType);
    void updateFirmware(const QString &deviceAddress,
                        const QString &newFirmwareHash,
                        int newVersion);
    void deactivateDevice(const QString &deviceAddress);
    void reactivateDevice(const QString &deviceAddress);

    // ── SensorConsensus — read ───────────────────────────────────
    void getLatestRound();
    void getConsensusStats();
    void getLatestConsensus();
    void getAllRounds();
    void getCurrentRound();
    void getConsensusRound(int roundId);
    void getReading(int roundId, const QString &sensorAddress);
    void getEventHistory(int fromBlock = 0);

    // ── SensorConsensus — write ──────────────────────────────────
    void submitReading(const QString &sensorAddress,
                       int value,
                       const QString &firmwareHash);
    void forceNewRound();
    void forceConsensus();
    void setFaultyThreshold(int threshold);
    void setMinSensors(int minSensors);
    void setConsensusWindow(int windowSeconds);
    void setDeviceRegistry(const QString &registryAddress);

    // ── Consensus explanation (NEW) ──────────────────────────────
    void getConsensusExplain(int roundId);

    // ── Blockchain inspector (NEW) ───────────────────────────────
    void getBlockchainNetwork();
    void getBlockchainLatest();
    void getBlockchainBlock(int blockNumber);
    void getBlockchainTx(const QString &txHash);

signals:
    // ── DeviceRegistry — read signals ────────────────────────────
    void registryStatsReady(QJsonObject data);
    void allDevicesReady(QJsonArray data);
    void deviceInfoReady(QJsonObject data);
    void verifyDeviceReady(QString address, bool isActive);
    void isDeviceRegisteredReady(QString address, bool isRegistered);
    void deviceFirmwareReady(QJsonObject data);
    void verifyFirmwareReady(QString address, bool matches);

    // ── DeviceRegistry — write signals ───────────────────────────
    void deviceRegistered(QJsonObject txData);
    void firmwareUpdated(QJsonObject txData);
    void deviceDeactivated(QJsonObject txData);
    void deviceReactivated(QJsonObject txData);

    // ── SensorConsensus — read signals ───────────────────────────
    void latestRoundReady(QJsonObject data);
    void consensusStatsReady(QJsonObject data);
    void latestConsensusReady(QJsonObject data);
    void allRoundsReady(QJsonArray data);
    void currentRoundReady(QJsonObject data);
    void consensusRoundReady(QJsonObject data);
    void readingReady(QJsonObject data);
    void eventHistoryReady(QJsonObject data);

    // ── SensorConsensus — write signals ──────────────────────────
    void readingSubmitted(QJsonObject txData);
    void newRoundStarted(QJsonObject txData);
    void consensusForced(QJsonObject txData);
    void faultyThresholdSet(QJsonObject txData);
    void minSensorsSet(QJsonObject txData);
    void consensusWindowSet(QJsonObject txData);
    void deviceRegistrySet(QJsonObject txData);

    // ── Consensus explanation (NEW) ──────────────────────────────
    void consensusExplainReady(QJsonObject data);

    // ── Blockchain inspector (NEW) ───────────────────────────────
    void blockchainNetworkReady(QJsonObject data);
    void blockchainLatestReady(QJsonObject data);
    void blockchainBlockReady(QJsonObject data);
    void blockchainTxReady(QJsonObject data);

    // ── Error signal ─────────────────────────────────────────────
    void requestError(QString endpoint, QString errorMessage);

private:
    static const QString BASE_URL;
    QNetworkAccessManager *m_manager;

    void get(const QString &endpoint,
             std::function<void(QJsonObject)> handler);
    void post(const QString &endpoint,
              const QJsonObject &body,
              std::function<void(QJsonObject)> handler);
    void handleReply(QNetworkReply *reply,
                     const QString &endpoint,
                     std::function<void(QJsonObject)> handler);
};
