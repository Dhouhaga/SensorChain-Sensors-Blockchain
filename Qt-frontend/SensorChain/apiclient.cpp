#include "apiclient.h"
#include <QJsonArray>

const QString ApiClient::BASE_URL = "http://localhost:3000";

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{}


void ApiClient::get(const QString &endpoint, std::function<void(QJsonObject)> handler)
{
    QNetworkRequest request(QUrl(BASE_URL + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_manager->get(request);
    handleReply(reply, endpoint, handler);
}

void ApiClient::post(const QString &endpoint, const QJsonObject &body,
                     std::function<void(QJsonObject)> handler)
{
    QNetworkRequest request(QUrl(BASE_URL + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_manager->post(request, data);
    handleReply(reply, endpoint, handler);
}

void ApiClient::handleReply(QNetworkReply *reply, const QString &endpoint,
                            std::function<void(QJsonObject)> handler)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint, handler]() {
        QByteArray raw = reply->readAll();

        qDebug() << "=== API RESPONSE from" << endpoint << "===";
        qDebug() << "HTTP Status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Raw response:" << raw;

        QJsonDocument doc = QJsonDocument::fromJson(raw);

        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            qDebug() << "Parsed JSON:" << obj;
            handler(obj);
            reply->deleteLater();
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            emit requestError(endpoint, reply->errorString());
        } else {
            emit requestError(endpoint, "Invalid JSON response");
        }

        reply->deleteLater();
    });
}
// DEVICE REGISTRY  READ

void ApiClient::getRegistryStats()
{
    get("/registry/stats", [this](QJsonObject obj) {
        emit registryStatsReady(obj["data"].toObject());
    });
}

void ApiClient::getAllDevices()
{
    get("/registry/devices", [this](QJsonObject obj) {
        emit allDevicesReady(obj["data"].toArray());
    });
}

void ApiClient::getDeviceInfo(const QString &address)
{
    get("/registry/devices/" + address, [this](QJsonObject obj) {
        emit deviceInfoReady(obj["data"].toObject());
    });
}

void ApiClient::verifyDevice(const QString &address)
{
    get("/registry/devices/" + address + "/verify", [this, address](QJsonObject obj) {
        emit verifyDeviceReady(address, obj["data"].toObject()["isActive"].toBool());
    });
}

void ApiClient::isDeviceRegistered(const QString &address)
{
    get("/registry/devices/" + address + "/registered", [this, address](QJsonObject obj) {
        emit isDeviceRegisteredReady(address, obj["data"].toObject()["isRegistered"].toBool());
    });
}

void ApiClient::getDeviceFirmware(const QString &address)
{
    get("/registry/devices/" + address + "/firmware", [this](QJsonObject obj) {
        emit deviceFirmwareReady(obj["data"].toObject());
    });
}

void ApiClient::verifyFirmware(const QString &address, const QString &hash)
{
    get("/registry/devices/" + address + "/verify-firmware?hash=" + hash,
        [this, address](QJsonObject obj) {
            emit verifyFirmwareReady(address, obj["data"].toObject()["matches"].toBool());
        });
}

// DEVICE REGISTRY  WRITE

void ApiClient::registerDevice(const QString &deviceAddress, const QString &firmwareHash,
                                int firmwareVersion, const QString &deviceType)
{
    QJsonObject body;
    body["deviceAddress"]   = deviceAddress;
    body["firmwareHash"]    = firmwareHash;
    body["firmwareVersion"] = firmwareVersion;
    body["deviceType"]      = deviceType;

    post("/registry/devices/register", body, [this](QJsonObject obj) {
        emit deviceRegistered(obj["data"].toObject());
    });
}

void ApiClient::updateFirmware(const QString &deviceAddress, const QString &newFirmwareHash,
                                int newVersion)
{
    QJsonObject body;
    body["newFirmwareHash"] = newFirmwareHash;
    body["newVersion"]      = newVersion;

    post("/registry/devices/" + deviceAddress + "/update-firmware", body,
         [this](QJsonObject obj) {
             emit firmwareUpdated(obj["data"].toObject());
         });
}

void ApiClient::deactivateDevice(const QString &deviceAddress)
{
    post("/registry/devices/" + deviceAddress + "/deactivate", {},
         [this](QJsonObject obj) {
             emit deviceDeactivated(obj["data"].toObject());
         });
}

void ApiClient::reactivateDevice(const QString &deviceAddress)
{
    post("/registry/devices/" + deviceAddress + "/reactivate", {},
         [this](QJsonObject obj) {
             emit deviceReactivated(obj["data"].toObject());
         });
}

// SENSOR CONSENSUS  READ

void ApiClient::getLatestRound()
{
    get("/consensus/latest-round", [this](QJsonObject obj) {
        emit latestRoundReady(obj["data"].toObject());
    });
}

void ApiClient::getConsensusStats()
{
    get("/consensus/stats", [this](QJsonObject obj) {
        emit consensusStatsReady(obj["data"].toObject());
    });
}

void ApiClient::getLatestConsensus()
{
    get("/consensus/latest", [this](QJsonObject obj) {
        emit latestConsensusReady(obj["data"].toObject());
    });
}

void ApiClient::getAllRounds()
{
    get("/consensus/rounds", [this](QJsonObject obj) {
        emit allRoundsReady(obj["data"].toArray());
    });
}

void ApiClient::getCurrentRound()
{
    get("/consensus/rounds/current", [this](QJsonObject obj) {
        emit currentRoundReady(obj["data"].toObject());
    });
}

void ApiClient::getConsensusRound(int roundId)
{
    get("/consensus/rounds/" + QString::number(roundId), [this](QJsonObject obj) {
        emit consensusRoundReady(obj["data"].toObject());
    });
}

void ApiClient::getReading(int roundId, const QString &sensorAddress)
{
    get("/consensus/rounds/" + QString::number(roundId) + "/reading/" + sensorAddress,
        [this](QJsonObject obj) {
            emit readingReady(obj["data"].toObject());
        });
}

void ApiClient::getEventHistory(int fromBlock)
{
    get("/consensus/events?fromBlock=" + QString::number(fromBlock),
        [this](QJsonObject obj) {
            emit eventHistoryReady(obj["data"].toObject());
        });
}

// SENSOR CONSENSUS  WRITE (sensor)

void ApiClient::submitReading(const QString &sensorAddress, int value,
                              const QString &firmwareHash)
{
    QJsonObject body;
    body["sensorAddress"] = sensorAddress;
    body["value"]         = value;
    body["firmwareHash"]  = firmwareHash;

    post("/consensus/submit", body, [this](QJsonObject obj) {
        // Pass the ENTIRE response object, not just obj["data"]
        // Because error responses don't have a "data" field
        emit readingSubmitted(obj);
    });
}
// SENSOR CONSENSUS  WRITE (admin)

void ApiClient::forceNewRound()
{
    post("/consensus/force-new-round", {}, [this](QJsonObject obj) {
        emit newRoundStarted(obj["data"].toObject());
    });
}

void ApiClient::forceConsensus()
{
    post("/consensus/force-consensus", {}, [this](QJsonObject obj) {
        emit consensusForced(obj["data"].toObject());
    });
}

void ApiClient::setFaultyThreshold(int threshold)
{
    QJsonObject body;
    body["threshold"] = threshold;
    post("/consensus/settings/faulty-threshold", body, [this](QJsonObject obj) {
        emit faultyThresholdSet(obj["data"].toObject());
    });
}

void ApiClient::setMinSensors(int minSensors)
{
    QJsonObject body;
    body["minSensors"] = minSensors;
    post("/consensus/settings/min-sensors", body, [this](QJsonObject obj) {
        emit minSensorsSet(obj["data"].toObject());
    });
}

void ApiClient::setConsensusWindow(int windowSeconds)
{
    QJsonObject body;
    body["windowSeconds"] = windowSeconds;
    post("/consensus/settings/consensus-window", body, [this](QJsonObject obj) {
        emit consensusWindowSet(obj["data"].toObject());
    });
}

void ApiClient::setDeviceRegistry(const QString &registryAddress)
{
    QJsonObject body;
    body["registryAddress"] = registryAddress;
    post("/consensus/settings/device-registry", body, [this](QJsonObject obj) {
        emit deviceRegistrySet(obj["data"].toObject());
    });
}


void ApiClient::getConsensusExplain(int roundId)
{
    get("/consensus/rounds/" + QString::number(roundId) + "/explain",
        [this](QJsonObject obj) {
            emit consensusExplainReady(obj["data"].toObject());
        });
}


void ApiClient::getBlockchainNetwork()
{
    get("/blockchain/network", [this](QJsonObject obj) {
        emit blockchainNetworkReady(obj["data"].toObject());
    });
}

void ApiClient::getBlockchainLatest()
{
    get("/blockchain/latest", [this](QJsonObject obj) {
        emit blockchainLatestReady(obj["data"].toObject());
    });
}

void ApiClient::getBlockchainBlock(int blockNumber)
{
    get("/blockchain/block/" + QString::number(blockNumber), [this](QJsonObject obj) {
        emit blockchainBlockReady(obj["data"].toObject());
    });
}

void ApiClient::getBlockchainTx(const QString &txHash)
{
    get("/blockchain/tx/" + txHash, [this](QJsonObject obj) {
        emit blockchainTxReady(obj["data"].toObject());
    });
}
