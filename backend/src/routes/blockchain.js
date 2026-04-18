// ============================================================
// routes/blockchain.js  (NEW)
// Blockchain explorer endpoints — blocks, transactions, network.
// ============================================================

const express = require("express");
const router  = express.Router();
const bc      = require("../services/blockchain");

const wrap = fn => (req, res) =>
  fn(req, res).catch(err => {
    console.error(err);
    res.status(500).json({ success: false, error: err.reason || err.message });
  });

// ──────────────────────────────────────────────────────────────
// GET /blockchain/network
// Chain ID, RPC URL, latest block number, gas price.
// ──────────────────────────────────────────────────────────────
router.get("/network", wrap(async (req, res) => {
  const info = await bc.getNetworkInfo();
  res.json({ success: true, data: info });
}));

// ──────────────────────────────────────────────────────────────
// GET /blockchain/latest
// Most recent block: number, hash, timestamp, tx count.
// ──────────────────────────────────────────────────────────────
router.get("/latest", wrap(async (req, res) => {
  const block = await bc.getLatestBlock();
  res.json({ success: true, data: block });
}));

// ──────────────────────────────────────────────────────────────
// GET /blockchain/block/:number
// Full block with all transactions.
// ──────────────────────────────────────────────────────────────
router.get("/block/:number", wrap(async (req, res) => {
  const number = Number(req.params.number);
  if (isNaN(number) || number < 0) {
    return res.status(400).json({ success: false, error: "Invalid block number" });
  }
  const block = await bc.getBlockByNumber(number);
  res.json({ success: true, data: block });
}));

// ──────────────────────────────────────────────────────────────
// GET /blockchain/tx/:hash
// Full transaction: tx + receipt + decoded logs.
// ──────────────────────────────────────────────────────────────
router.get("/tx/:hash", wrap(async (req, res) => {
  const details = await bc.getTransactionDetails(req.params.hash);
  res.json({ success: true, data: details });
}));

module.exports = router;
