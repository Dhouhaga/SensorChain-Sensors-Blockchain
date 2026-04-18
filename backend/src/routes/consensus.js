// ============================================================
// routes/consensus.js — All SensorConsensus endpoints
// ============================================================

const express = require("express");
const router = express.Router();
const bc = require("../services/blockchain");

// ── Helper: wrap async route handlers ───────────────────────
const wrap = fn => (req, res) =>
  fn(req, res).catch(err => {
    console.error(err);
    res.status(500).json({ success: false, error: err.reason || err.message });
  });

// ============================================================
// READ endpoints
// ============================================================

/**
 * GET /consensus/stats
 * Returns all contract state variables in one call:
 * currentRoundId, consensusWindow, minSensors, faultyThreshold,
 * totalRounds, owner, roundTimeRemainingSeconds, etc.
 */
router.get("/stats", wrap(async (req, res) => {
  const stats = await bc.getConsensusStats();
  res.json({ success: true, data: stats });
}));

/**
 * GET /consensus/latest
 * Returns the most recently finalised trusted consensus value.
 */
router.get("/latest", wrap(async (req, res) => {
  const latest = await bc.getLatestConsensus();
  res.json({ success: true, data: latest });
}));

/**
 * GET /consensus/latest-round
 * Returns the most recent round result — successful OR rejected.
 * Unlike /consensus/latest which skips rejected rounds, this always
 * returns what actually happened last.
 */
router.get("/latest-round", wrap(async (req, res) => {
  const data = await bc.getLatestRound();
  res.json({ success: true, data });
}));

/**
 * GET /consensus/rounds
 * Returns all rounds from round 1 to currentRoundId.
 */
router.get("/rounds", wrap(async (req, res) => {
  const rounds = await bc.getAllRounds();
  res.json({ success: true, count: rounds.length, data: rounds });
}));

/**
 * GET /consensus/rounds/current
 * Returns the current round ID and its participants.
 */
router.get("/rounds/current", wrap(async (req, res) => {
  const [stats, participantDetails] = await Promise.all([
    bc.getConsensusStats(),
    bc.getCurrentRoundDetails()   // single contract call — returns addresses + values + flags
  ]);

  res.json({
    success: true,
    data: {
      currentRoundId: stats.currentRoundId,
      participants: participantDetails.map(d => d.address),
      participantDetails,
      participantCount: participantDetails.length,
      minSensorsRequired: stats.minSensorsForConsensus,
      roundTimeRemainingSeconds: stats.roundTimeRemainingSeconds
    }
  });
}));

/**
 * GET /consensus/rounds/:id
 * Returns full detail for a specific round including per-sensor
 * values, disagreement scores, and faulty flags.
 */
router.get("/rounds/:id", wrap(async (req, res) => {
  const round = await bc.getConsensusRound(Number(req.params.id));
  res.json({ success: true, data: round });
}));

/**
 * GET /consensus/rounds/:id/reading/:sensor
 * Returns the reading submitted by a specific sensor in a specific round.
 */
router.get("/rounds/:id/reading/:sensor", wrap(async (req, res) => {
  const reading = await bc.getReading(Number(req.params.id), req.params.sensor);
  res.json({ success: true, data: reading });
}));

/**
 * GET /consensus/events?fromBlock=0
 * Returns full event history from both contracts.
 * Optional query param: fromBlock (default 0)
 */
router.get("/events", wrap(async (req, res) => {
  const fromBlock = req.query.fromBlock ? Number(req.query.fromBlock) : 0;
  const events = await bc.getEventHistory(fromBlock);
  res.json({ success: true, data: events });
}));

// ============================================================
// WRITE endpoints — sensor submissions
// ============================================================

/**
 * POST /consensus/submit
 * Submit a reading on behalf of a sensor device.
 * The backend handles signing — no signature needed from the caller.
 *
 * Body: { sensorAddress, value, firmwareHash }
 *   sensorAddress — must match a key in SENSOR_PRIVATE_KEY_<address> env var
 *   value         — integer scaled x100 (e.g. 2550 for 25.50 C)
 *   firmwareHash  — bytes32 hex string matching the registered firmware hash
 */
router.post("/submit", wrap(async (req, res) => {
  const { sensorAddress, value, firmwareHash } = req.body;
  if (!sensorAddress || value === undefined || value === null || !firmwareHash) {
    return res.status(400).json({ success: false, error: "sensorAddress, value, and firmwareHash are required" });
  }
  const result = await bc.submitReading(sensorAddress, Number(value), firmwareHash);
  res.json({ success: true, message: "Reading submitted successfully", data: result });
}));

// ============================================================
// WRITE endpoints — admin / owner only
// ============================================================

/**
 * POST /consensus/force-new-round
 * Closes the current round and opens a new one.
 */
router.post("/force-new-round", wrap(async (req, res) => {
  const receipt = await bc.forceNewRound();
  res.json({ success: true, message: "New round started", data: receipt });
}));

/**
 * POST /consensus/force-consensus
 * Manually triggers consensus calculation for the current round.
 */
router.post("/force-consensus", wrap(async (req, res) => {
  const receipt = await bc.forceConsensusCalculation();
  res.json({ success: true, message: "Consensus calculated", data: receipt });
}));

/**
 * POST /consensus/settings/faulty-threshold
 * Body: { threshold } — integer scaled x100 (e.g. 500 = 5.00 degrees)
 */
router.post("/settings/faulty-threshold", wrap(async (req, res) => {
  const { threshold } = req.body;
  if (!threshold) return res.status(400).json({ success: false, error: "threshold is required" });
  const receipt = await bc.setFaultyThreshold(Number(threshold));
  res.json({ success: true, message: `Faulty threshold set to ${threshold}`, data: receipt });
}));

/**
 * POST /consensus/settings/min-sensors
 * Body: { minSensors } — minimum number of sensors required (>= 2)
 */
router.post("/settings/min-sensors", wrap(async (req, res) => {
  const { minSensors } = req.body;
  if (!minSensors) return res.status(400).json({ success: false, error: "minSensors is required" });
  const receipt = await bc.setMinSensorsForConsensus(Number(minSensors));
  res.json({ success: true, message: `Min sensors set to ${minSensors}`, data: receipt });
}));

/**
 * POST /consensus/settings/consensus-window
 * Body: { windowSeconds } — round duration in seconds (>= 60)
 */
router.post("/settings/consensus-window", wrap(async (req, res) => {
  const { windowSeconds } = req.body;
  if (!windowSeconds) return res.status(400).json({ success: false, error: "windowSeconds is required" });
  const receipt = await bc.setConsensusWindow(Number(windowSeconds));
  res.json({ success: true, message: `Consensus window set to ${windowSeconds}s`, data: receipt });
}));

/**
 * POST /consensus/settings/device-registry
 * Body: { registryAddress } — address of a new DeviceRegistry deployment
 */
router.post("/settings/device-registry", wrap(async (req, res) => {
  const { registryAddress } = req.body;
  if (!registryAddress) return res.status(400).json({ success: false, error: "registryAddress is required" });
  const receipt = await bc.setDeviceRegistry(registryAddress);
  res.json({ success: true, message: "Device registry updated", data: receipt });
}));

module.exports = router;
