// ============================================================
// routes/registry.js — All DeviceRegistry endpoints
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
 * GET /registry/stats
 * Returns contract address, owner, and total device count.
 */
router.get("/stats", wrap(async (req, res) => {
  const stats = await bc.getRegistryStats();
  res.json({ success: true, data: stats });
}));

/**
 * GET /registry/devices
 * Returns full info for every registered device.
 */
router.get("/devices", wrap(async (req, res) => {
  const devices = await bc.getAllDevices();
  res.json({ success: true, count: devices.length, data: devices });
}));

/**
 * GET /registry/devices/:address
 * Returns full info for a single device.
 */
router.get("/devices/:address", wrap(async (req, res) => {
  const device = await bc.getDeviceInfo(req.params.address);
  res.json({ success: true, data: device });
}));

/**
 * GET /registry/devices/:address/verify
 * Returns true if the device is registered AND active.
 */
router.get("/devices/:address/verify", wrap(async (req, res) => {
  const isValid = await bc.verifyDevice(req.params.address);
  res.json({ success: true, data: { address: req.params.address, isActive: isValid } });
}));

/**
 * GET /registry/devices/:address/registered
 * Returns true if the address has ever been registered (active or not).
 */
router.get("/devices/:address/registered", wrap(async (req, res) => {
  const isRegistered = await bc.isDeviceRegistered(req.params.address);
  res.json({ success: true, data: { address: req.params.address, isRegistered } });
}));

/**
 * GET /registry/devices/:address/firmware
 * Returns firmware hash, version, and last update timestamp.
 */
router.get("/devices/:address/firmware", wrap(async (req, res) => {
  const firmware = await bc.getDeviceFirmware(req.params.address);
  res.json({ success: true, data: firmware });
}));

/**
 * GET /registry/devices/:address/verify-firmware?hash=0x...
 * Returns true if the supplied hash matches the on-chain record.
 */
router.get("/devices/:address/verify-firmware", wrap(async (req, res) => {
  const { hash } = req.query;
  if (!hash) return res.status(400).json({ success: false, error: "hash query param required" });
  const matches = await bc.verifyFirmware(req.params.address, hash);
  res.json({ success: true, data: { address: req.params.address, firmwareHash: hash, matches } });
}));

// ============================================================
// WRITE endpoints (admin / owner only)
// ============================================================

/**
 * POST /registry/devices/register
 * Body: { deviceAddress, firmwareHash, firmwareVersion, deviceType }
 */
router.post("/devices/register", wrap(async (req, res) => {
  const { deviceAddress, firmwareHash, firmwareVersion, deviceType } = req.body;
  if (!deviceAddress || !firmwareHash || !firmwareVersion || !deviceType) {
    return res.status(400).json({ success: false, error: "deviceAddress, firmwareHash, firmwareVersion, deviceType are required" });
  }
  const receipt = await bc.registerDevice(deviceAddress, firmwareHash, Number(firmwareVersion), deviceType);
  res.json({ success: true, message: "Device registered successfully", data: receipt });
}));

/**
 * POST /registry/devices/:address/update-firmware
 * Body: { newFirmwareHash, newVersion }
 */
router.post("/devices/:address/update-firmware", wrap(async (req, res) => {
  const { newFirmwareHash, newVersion } = req.body;
  if (!newFirmwareHash || !newVersion) {
    return res.status(400).json({ success: false, error: "newFirmwareHash and newVersion are required" });
  }
  const receipt = await bc.updateFirmware(req.params.address, newFirmwareHash, Number(newVersion));
  res.json({ success: true, message: "Firmware updated successfully", data: receipt });
}));

/**
 * POST /registry/devices/:address/deactivate
 * No body required.
 */
router.post("/devices/:address/deactivate", wrap(async (req, res) => {
  const receipt = await bc.deactivateDevice(req.params.address);
  res.json({ success: true, message: "Device deactivated", data: receipt });
}));

/**
 * POST /registry/devices/:address/reactivate
 * No body required.
 */
router.post("/devices/:address/reactivate", wrap(async (req, res) => {
  const receipt = await bc.reactivateDevice(req.params.address);
  res.json({ success: true, message: "Device reactivated", data: receipt });
}));

module.exports = router;
