// routes/consensus.js  All SensorConsensus endpoints

const express = require("express");
const router  = express.Router();
const bc      = require("../services/blockchain");

const wrap = fn => (req, res) =>
  fn(req, res).catch(err => {
    console.error("API Error:", err);
    
    // Check if this is a formatted blockchain error (from blockchain.js)
    if (err.formattedResponse) {
      return res.status(400).json(err.formattedResponse);
    }
    
    if (err.reason) {
      return res.status(400).json({
        success: false,
        error: ` Blockchain error: ${err.reason}`,
        action: "Please check sensor status and try again.",
        type: "blockchain",
        details: {
          reason: err.reason,
          transactionHash: err.transactionHash || null,
          blockchainError: true
        }
      });
    }
    
    res.status(500).json({ 
      success: false, 
      error: err.message || "Internal server error",
      details: { blockchainError: false }
    });
  });


router.get("/stats", wrap(async (req, res) => {
  const stats = await bc.getConsensusStats();
  res.json({ success: true, data: stats });
}));

router.get("/latest", wrap(async (req, res) => {
  const latest = await bc.getLatestConsensus();
  res.json({ success: true, data: latest });
}));

router.get("/latest-round", wrap(async (req, res) => {
  const data = await bc.getLatestRound();
  res.json({ success: true, data });
}));

router.get("/rounds", wrap(async (req, res) => {
  const rounds = await bc.getAllRounds();
  res.json({ success: true, count: rounds.length, data: rounds });
}));

router.get("/rounds/current", wrap(async (req, res) => {
  const [stats, participantDetails] = await Promise.all([
    bc.getConsensusStats(),
    bc.getCurrentRoundDetails()
  ]);
  res.json({
    success: true,
    data: {
      currentRoundId:           stats.currentRoundId,
      participants:             participantDetails.map(d => d.address),
      participantDetails,
      participantCount:         participantDetails.length,
      minSensorsRequired:       stats.minSensorsForConsensus,
      roundTimeRemainingSeconds: stats.roundTimeRemainingSeconds
    }
  });
}));

router.get("/rounds/:id/explain", wrap(async (req, res) => {
  const id = Number(req.params.id);
  if (isNaN(id) || id < 1) {
    return res.status(400).json({ success: false, error: "Invalid round ID" });
  }
  const explanation = await bc.explainConsensusRound(id);
  res.json({ success: true, data: explanation });
}));

router.get("/rounds/:id", wrap(async (req, res) => {
  const round = await bc.getConsensusRound(Number(req.params.id));
  res.json({ success: true, data: round });
}));

router.get("/rounds/:id/reading/:sensor", wrap(async (req, res) => {
  const reading = await bc.getReading(Number(req.params.id), req.params.sensor);
  res.json({ success: true, data: reading });
}));

router.get("/events", wrap(async (req, res) => {
  const fromBlock = req.query.fromBlock ? Number(req.query.fromBlock) : 0;
  const events    = await bc.getEventHistory(fromBlock);
  res.json({ success: true, data: events });
}));

// WRITE endpoints  sensor submissions

router.post("/submit", wrap(async (req, res) => {
  const { sensorAddress, value, firmwareHash } = req.body;
  
  if (!sensorAddress || value === undefined || value === null || !firmwareHash) {
    return res.status(400).json({ 
      success: false, 
      error: "Missing required fields",
      action: "Please provide sensorAddress, value, and firmwareHash",
      type: "validation"
    });
  }
  
  const result = await bc.submitReading(sensorAddress, Number(value), firmwareHash);
  res.json({ success: true, message: "Reading submitted successfully", data: result });
}));

// WRITE endpoints  admin / owner only

router.post("/force-new-round", wrap(async (req, res) => {
  const receipt = await bc.forceNewRound();
  res.json({ success: true, message: "New round started", data: receipt });
}));

router.post("/force-consensus", wrap(async (req, res) => {
  const receipt = await bc.forceConsensusCalculation();
  res.json({ success: true, message: "Consensus calculated", data: receipt });
}));

router.post("/settings/faulty-threshold", wrap(async (req, res) => {
  const { threshold } = req.body;
  if (!threshold) return res.status(400).json({ success: false, error: "threshold is required" });
  const receipt = await bc.setFaultyThreshold(Number(threshold));
  res.json({ success: true, message: `Faulty threshold set to ${threshold}`, data: receipt });
}));

router.post("/settings/min-sensors", wrap(async (req, res) => {
  const { minSensors } = req.body;
  if (!minSensors) return res.status(400).json({ success: false, error: "minSensors is required" });
  const receipt = await bc.setMinSensorsForConsensus(Number(minSensors));
  res.json({ success: true, message: `Min sensors set to ${minSensors}`, data: receipt });
}));

router.post("/settings/consensus-window", wrap(async (req, res) => {
  const { windowSeconds } = req.body;
  if (!windowSeconds) return res.status(400).json({ success: false, error: "windowSeconds is required" });
  const receipt = await bc.setConsensusWindow(Number(windowSeconds));
  res.json({ success: true, message: `Consensus window set to ${windowSeconds}s`, data: receipt });
}));

router.post("/settings/device-registry", wrap(async (req, res) => {
  const { registryAddress } = req.body;
  if (!registryAddress) return res.status(400).json({ success: false, error: "registryAddress is required" });
  const receipt = await bc.setDeviceRegistry(registryAddress);
  res.json({ success: true, message: "Device registry updated", data: receipt });
}));

module.exports = router;