// ============================================================
// index.js — Express server entry point
// ============================================================

require("dotenv").config();
const express = require("express");
const cors = require("cors");

const registryRoutes = require("./routes/registry");
const consensusRoutes = require("./routes/consensus");

const app = express();

// ── Middleware ───────────────────────────────────────────────
app.use(cors());
app.use(express.json());

// ── Request logger ───────────────────────────────────────────
app.use((req, res, next) => {
  console.log(`[${new Date().toISOString()}] ${req.method} ${req.path}`);
  next();
});

// ── Routes ───────────────────────────────────────────────────
app.use("/registry", registryRoutes);
app.use("/consensus", consensusRoutes);

// ── Health check ─────────────────────────────────────────────
app.get("/health", (req, res) => {
  res.json({
    status: "ok",
    timestamp: new Date().toISOString(),
    contracts: {
      deviceRegistry: process.env.DEVICE_REGISTRY_ADDRESS,
      sensorConsensus: process.env.SENSOR_CONSENSUS_ADDRESS
    }
  });
});

// ── API reference ────────────────────────────────────────────
app.get("/", (req, res) => {
  res.json({
    name: "IoT Blockchain Backend",
    version: "1.0.0",
    endpoints: {
      health: "GET /health",
      registry: {
        stats:           "GET  /registry/stats",
        allDevices:      "GET  /registry/devices",
        deviceInfo:      "GET  /registry/devices/:address",
        verifyDevice:    "GET  /registry/devices/:address/verify",
        isRegistered:    "GET  /registry/devices/:address/registered",
        firmware:        "GET  /registry/devices/:address/firmware",
        verifyFirmware:  "GET  /registry/devices/:address/verify-firmware?hash=0x...",
        register:        "POST /registry/devices/register",
        updateFirmware:  "POST /registry/devices/:address/update-firmware",
        deactivate:      "POST /registry/devices/:address/deactivate",
        reactivate:      "POST /registry/devices/:address/reactivate"
      },
      consensus: {
        stats:           "GET  /consensus/stats",
        latest:          "GET  /consensus/latest",
        latestRound:     "GET  /consensus/latest-round",
        allRounds:       "GET  /consensus/rounds",
        currentRound:    "GET  /consensus/rounds/current",
        roundById:       "GET  /consensus/rounds/:id",
        readingBySensor: "GET  /consensus/rounds/:id/reading/:sensor",
        events:          "GET  /consensus/events?fromBlock=0",
        submit:          "POST /consensus/submit",
        forceNewRound:   "POST /consensus/force-new-round",
        forceConsensus:  "POST /consensus/force-consensus",
        setThreshold:    "POST /consensus/settings/faulty-threshold",
        setMinSensors:   "POST /consensus/settings/min-sensors",
        setWindow:       "POST /consensus/settings/consensus-window",
        setRegistry:     "POST /consensus/settings/device-registry"
      }
    }
  });
});

// ── 404 handler ──────────────────────────────────────────────
app.use((req, res) => {
  res.status(404).json({ success: false, error: `Route ${req.method} ${req.path} not found` });
});

// ── Global error handler ─────────────────────────────────────
app.use((err, req, res, next) => {
  console.error(err);
  res.status(500).json({ success: false, error: err.message });
});

// ── Start ────────────────────────────────────────────────────
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  console.log("  IoT Blockchain Backend");
  console.log(`  Running on http://localhost:${PORT}`);
  console.log(`  Ganache RPC: ${process.env.RPC_URL}`);
  console.log(`  DeviceRegistry:   ${process.env.DEVICE_REGISTRY_ADDRESS}`);
  console.log(`  SensorConsensus:  ${process.env.SENSOR_CONSENSUS_ADDRESS}`);
  console.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  console.log(`  API reference:    http://localhost:${PORT}/`);
  console.log(`  Health check:     http://localhost:${PORT}/health`);
  console.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
});
