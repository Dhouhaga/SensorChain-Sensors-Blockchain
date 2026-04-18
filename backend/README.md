# IoT Blockchain Backend

Node.js/Express backend that sits between your Qt dashboard and the Ethereum contracts.
It handles all Web3 interaction — signing, ABI encoding, gas — so Qt only deals with simple HTTP calls.

---

## Project Structure

```
backend/
├── src/
│   ├── index.js                  ← Express server, route mounting
│   ├── config/
│   │   └── abis.js               ← Contract ABIs
│   ├── routes/
│   │   ├── registry.js           ← DeviceRegistry endpoints
│   │   └── consensus.js          ← SensorConsensus endpoints
│   └── services/
│       └── blockchain.js         ← All ethers.js logic
├── .env.example                  ← Environment variable template
├── package.json
└── README.md
```

---

## Setup

### 1. Install dependencies
```bash
cd backend
npm install
```

### 2. Configure environment
```bash
cp .env.example .env
```

Open `.env` and fill in:
- Your Ganache RPC URL (default: `http://127.0.0.1:7545`)
- Your deployed contract addresses from Remix
- Your owner private key (Ganache account 0)
- Your sensor private keys (one per registered sensor)

```env
RPC_URL=http://127.0.0.1:7545
DEVICE_REGISTRY_ADDRESS=0x...
SENSOR_CONSENSUS_ADDRESS=0x...
OWNER_PRIVATE_KEY=0x...
SENSOR_PRIVATE_KEY_0xAAc60b...=0x...
SENSOR_PRIVATE_KEY_0x63098B...=0x...
```

### 3. Start Ganache
Make sure Ganache is running on port 7545 before starting the server.

### 4. Start the server
```bash
# Production
npm start

# Development (auto-restarts on file change)
npm run dev
```

You should see:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  IoT Blockchain Backend
  Running on http://localhost:3000
  Ganache RPC: http://127.0.0.1:7545
  DeviceRegistry:   0x...
  SensorConsensus:  0x...
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## API Reference

Visit `http://localhost:3000/` for the full endpoint map.

---

### DeviceRegistry Endpoints

| Method | URL | Description |
|--------|-----|-------------|
| GET | `/registry/stats` | Contract owner, address, total devices |
| GET | `/registry/devices` | All registered devices with full info |
| GET | `/registry/devices/:address` | Single device full info |
| GET | `/registry/devices/:address/verify` | Is device registered AND active? |
| GET | `/registry/devices/:address/registered` | Has device ever been registered? |
| GET | `/registry/devices/:address/firmware` | Firmware hash, version, last update |
| GET | `/registry/devices/:address/verify-firmware?hash=0x...` | Does hash match on-chain record? |
| POST | `/registry/devices/register` | Register a new device (owner only) |
| POST | `/registry/devices/:address/update-firmware` | Update firmware record (owner only) |
| POST | `/registry/devices/:address/deactivate` | Deactivate device (owner only) |
| POST | `/registry/devices/:address/reactivate` | Reactivate device (owner only) |

#### POST /registry/devices/register
```json
{
  "deviceAddress": "0xAAc60b076c4D83f14c7cB3653146bF7395D08d77",
  "firmwareHash": "0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
  "firmwareVersion": 1,
  "deviceType": "Temperature Sensor"
}
```

#### POST /registry/devices/:address/update-firmware
```json
{
  "newFirmwareHash": "0xabcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
  "newVersion": 2
}
```

---

### SensorConsensus Endpoints

| Method | URL | Description |
|--------|-----|-------------|
| GET | `/consensus/stats` | All contract settings + round time remaining |
| GET | `/consensus/latest` | Most recent trusted consensus value |
| GET | `/consensus/rounds` | All rounds (full detail) |
| GET | `/consensus/rounds/current` | Current round participants and time left |
| GET | `/consensus/rounds/:id` | Full detail for a specific round |
| GET | `/consensus/rounds/:id/reading/:sensor` | One sensor's reading in one round |
| GET | `/consensus/events?fromBlock=0` | Full event history from both contracts |
| POST | `/consensus/submit` | Submit a sensor reading |
| POST | `/consensus/force-new-round` | Close round and open new one (owner only) |
| POST | `/consensus/force-consensus` | Force consensus calculation (owner only) |
| POST | `/consensus/settings/faulty-threshold` | Set fault detection threshold (owner only) |
| POST | `/consensus/settings/min-sensors` | Set minimum sensors required (owner only) |
| POST | `/consensus/settings/consensus-window` | Set round window duration (owner only) |
| POST | `/consensus/settings/device-registry` | Update registry address (owner only) |

#### POST /consensus/submit
```json
{
  "sensorAddress": "0xAAc60b076c4D83f14c7cB3653146bF7395D08d77",
  "value": 2550,
  "firmwareHash": "0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"
}
```
Note: value is scaled x100. 2550 = 25.50°C. The backend generates the timestamp and signature automatically.

#### POST /consensus/settings/faulty-threshold
```json
{ "threshold": 500 }
```
500 = 5.00 degrees per comparison.

#### POST /consensus/settings/min-sensors
```json
{ "minSensors": 3 }
```

#### POST /consensus/settings/consensus-window
```json
{ "windowSeconds": 3600 }
```

---

## Response Format

All endpoints return:
```json
{
  "success": true,
  "data": { ... }
}
```

On error:
```json
{
  "success": false,
  "error": "Human-readable error message from the contract or server"
}
```

Write operations also return a transaction receipt:
```json
{
  "transactionHash": "0x...",
  "blockNumber": 42,
  "gasUsed": "85000",
  "status": "success"
}
```

---

## How Qt connects to this

From your Qt C++ app, every contract interaction is just an HTTP call:

```cpp
// Submit a reading
QJsonObject body;
body["sensorAddress"] = "0xAAc60b...";
body["value"] = 2550;
body["firmwareHash"] = "0x1234...";

QNetworkRequest request(QUrl("http://localhost:3000/consensus/submit"));
request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
manager->post(request, QJsonDocument(body).toJson());

// Get latest consensus
QNetworkRequest request(QUrl("http://localhost:3000/consensus/latest"));
manager->get(request);
```

No private keys, no ABI encoding, no ethers.js in Qt. Just JSON over HTTP.
