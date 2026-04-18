// ============================================================
// blockchain.js — All ethers.js interaction in one place.
// Routes never touch ethers directly; they call this service.
// ============================================================

const { ethers } = require("ethers");
const { DEVICE_REGISTRY_ABI, SENSOR_CONSENSUS_ABI } = require("../config/abis");

const GAS_LIMIT = 1000000;

// ── Provider & signers ──────────────────────────────────────────────────────

const provider = new ethers.providers.JsonRpcProvider(process.env.RPC_URL);

const ownerWallet = new ethers.Wallet(process.env.OWNER_PRIVATE_KEY, provider);

const registryRead = new ethers.Contract(
  process.env.DEVICE_REGISTRY_ADDRESS,
  DEVICE_REGISTRY_ABI,
  provider
);

const consensusRead = new ethers.Contract(
  process.env.SENSOR_CONSENSUS_ADDRESS,
  SENSOR_CONSENSUS_ABI,
  provider
);

const registryWrite  = registryRead.connect(ownerWallet);
const consensusWrite = consensusRead.connect(ownerWallet);

// ── Helper: get a sensor wallet from env ────────────────────────────────────

function getSensorWallet(sensorAddress) {
  // Try exact match first, then case-insensitive search
  let key = process.env[`SENSOR_PRIVATE_KEY_${sensorAddress}`];

  if (!key) {
    // Case-insensitive fallback — env keys are sometimes stored lowercase
    const lowerAddr = sensorAddress.toLowerCase();
    for (const envKey of Object.keys(process.env)) {
      if (envKey.startsWith("SENSOR_PRIVATE_KEY_") &&
          envKey.replace("SENSOR_PRIVATE_KEY_", "").toLowerCase() === lowerAddr) {
        key = process.env[envKey];
        break;
      }
    }
  }

  if (!key) throw new Error(`No private key configured for sensor ${sensorAddress}`);
  return new ethers.Wallet(key, provider);
}

// ── Formatters ───────────────────────────────────────────────────────────────

function formatReceipt(receipt) {
  return {
    transactionHash: receipt.transactionHash,
    blockNumber: receipt.blockNumber,
    gasUsed: receipt.gasUsed.toString(),
    status: receipt.status === 1 ? "success" : "failed"
  };
}

function formatDevice(d) {
  return {
    deviceAddress: d.deviceAddress,
    firmwareHash: d.firmwareHash,
    firmwareVersion: d.firmwareVersion.toString(),
    deviceType: d.deviceType,
    isActive: d.isActive,
    registrationTime: d.registrationTime.toString(),
    lastUpdate: d.lastUpdate.toString()
  };
}

function formatRound(r) {
  return {
    roundId: r.roundId.toString(),
    timestamp: r.timestamp.toString(),
    consensusValue: r.consensusValue.toString(),
    consensusValueScaled: (Number(r.consensusValue) / 100).toFixed(2),
    totalParticipants: r.totalParticipants.toString(),
    trustedParticipants: r.trustedParticipants.toString(),
    faultyCount: r.faultyCount.toString(),
    consensusReached: r.consensusReached,
    participants: r.participants,
    values: r.values.map(v => v.toString()),
    valuesScaled: r.values.map(v => (Number(v) / 100).toFixed(2)),
    disagreementScores: r.disagreementScores.map(s => s.toString()),
    faultyFlags: r.faultyFlags,
    sensors: r.participants.map((addr, i) => ({
      address: addr,
      value: r.values[i].toString(),
      valueScaled: (Number(r.values[i]) / 100).toFixed(2),
      disagreementScore: r.disagreementScores[i].toString(),
      isFaulty: r.faultyFlags[i]
    }))
  };
}

// ============================================================
// DEVICE REGISTRY — WRITE
// ============================================================

async function registerDevice(deviceAddress, firmwareHash, firmwareVersion, deviceType) {
  const tx = await registryWrite.registerDevice(
    deviceAddress, firmwareHash, firmwareVersion, deviceType,
    { gasLimit: GAS_LIMIT }
  );
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

async function updateFirmware(deviceAddress, newFirmwareHash, newVersion) {
  const tx = await registryWrite.updateFirmware(
    deviceAddress, newFirmwareHash, newVersion,
    { gasLimit: GAS_LIMIT }
  );
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

async function deactivateDevice(deviceAddress) {
  const tx = await registryWrite.deactivateDevice(deviceAddress, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

async function reactivateDevice(deviceAddress) {
  const tx = await registryWrite.reactivateDevice(deviceAddress, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

// ============================================================
// DEVICE REGISTRY — READ
// ============================================================

async function getAllDevices() {
  const addresses = await registryRead.getAllDevices();
  const devices = await Promise.all(
    addresses.map(addr => registryRead.getDeviceInfo(addr).then(formatDevice))
  );
  return devices;
}

async function getDeviceInfo(deviceAddress) {
  const d = await registryRead.getDeviceInfo(deviceAddress);
  return formatDevice(d);
}

async function verifyDevice(deviceAddress) {
  return registryRead.verifyDevice(deviceAddress);
}

async function verifyFirmware(deviceAddress, firmwareHash) {
  return registryRead.verifyFirmware(deviceAddress, firmwareHash);
}

async function getDeviceFirmware(deviceAddress) {
  const result = await registryRead.getDeviceFirmware(deviceAddress);
  return {
    firmwareHash: result.firmwareHash,
    firmwareVersion: result.firmwareVersion.toString(),
    lastUpdate: result.lastUpdate.toString()
  };
}

async function isDeviceRegistered(deviceAddress) {
  return registryRead.isDeviceRegistered(deviceAddress);
}

async function getRegistryStats() {
  const [owner, total] = await Promise.all([
    registryRead.contractOwner(),
    registryRead.totalDevices()
  ]);
  return {
    contractOwner: owner,
    totalDevices: total.toString(),
    contractAddress: process.env.DEVICE_REGISTRY_ADDRESS
  };
}

// ============================================================
// SENSOR CONSENSUS — WRITE (sensor)
// ============================================================

async function submitReading(sensorAddress, value, firmwareHash) {
  const wallet = getSensorWallet(sensorAddress);
  const consensusWithSensor = consensusRead.connect(wallet);

  const timestamp = Math.floor(Date.now() / 1000);

  const messageHash = ethers.utils.solidityKeccak256(
    ["int256", "uint256"],
    [value, timestamp]
  );
  const signature = await wallet.signMessage(ethers.utils.arrayify(messageHash));

  const tx = await consensusWithSensor.submitReading(
    value,
    timestamp,
    firmwareHash,
    signature,
    { gasLimit: GAS_LIMIT }
  );
  const receipt = await tx.wait();

  return {
    ...formatReceipt(receipt),
    sensorAddress,
    value: value.toString(),
    valueScaled: (Number(value) / 100).toFixed(2),
    timestamp,
    firmwareHash
  };
}

// ============================================================
// SENSOR CONSENSUS — WRITE (admin)
// ============================================================

async function forceNewRound() {
  const tx = await consensusWrite.forceNewRound({ gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

async function forceConsensusCalculation() {
  const tx = await consensusWrite.forceConsensusCalculation({ gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

async function setFaultyThreshold(threshold) {
  const tx = await consensusWrite.setFaultyThreshold(threshold, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

async function setMinSensorsForConsensus(minSensors) {
  const tx = await consensusWrite.setMinSensorsForConsensus(minSensors, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

async function setConsensusWindow(windowSeconds) {
  const tx = await consensusWrite.setConsensusWindow(windowSeconds, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

async function setDeviceRegistry(newRegistryAddress) {
  const tx = await consensusWrite.setDeviceRegistry(newRegistryAddress, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatReceipt(receipt);
}

// ============================================================
// SENSOR CONSENSUS — READ
// ============================================================

async function getLatestConsensus() {
  const result = await consensusRead.getLatestConsensus();
  return {
    consensusValue: result.consensusValue.toString(),
    consensusValueScaled: (Number(result.consensusValue) / 100).toFixed(2),
    trustedCount: result.trustedCount.toString(),
    faultyCount: result.faultyCount.toString(),
    timestamp: result.timestamp.toString()
  };
}

async function getLatestRound() {
  // Instead of calling the getLatestRound() contract function (which has
  // ethers v5 tuple decoding issues), we reconstruct the answer ourselves
  // using getConsensusRound() which we know works correctly.
  try {
    const currentRoundId = await consensusRead.currentRoundId();
    const total = Number(currentRoundId);

    // Walk backwards from current round to find the last processed one
    for (let rid = total; rid >= 1; rid--) {
      try {
        const r = await consensusRead.getConsensusRound(rid);
        // A round is processed if it has a timestamp
        if (r.timestamp && r.timestamp.toString() !== "0") {
          return {
            roundId:             r.roundId.toString(),
            consensusReached:    r.consensusReached,
            consensusValue:      r.consensusValue.toString(),
            consensusValueScaled:(Number(r.consensusValue) / 100).toFixed(2),
            totalParticipants:   r.totalParticipants.toString(),
            trustedParticipants: r.trustedParticipants.toString(),
            faultyCount:         r.faultyCount.toString(),
            timestamp:           r.timestamp.toString(),
            noRoundsYet: false
          };
        }
      } catch {
        continue;
      }
    }

    // No processed rounds found
    return {
      roundId: "0", consensusReached: false, consensusValue: "0",
      consensusValueScaled: "0.00", totalParticipants: "0",
      trustedParticipants: "0", faultyCount: "0", timestamp: "0",
      noRoundsYet: true
    };
  } catch (err) {
    return {
      roundId: "0", consensusReached: false, consensusValue: "0",
      consensusValueScaled: "0.00", totalParticipants: "0",
      trustedParticipants: "0", faultyCount: "0", timestamp: "0",
      noRoundsYet: true
    };
  }
}

async function getConsensusRound(roundId) {
  const r = await consensusRead.getConsensusRound(roundId);
  return formatRound(r);
}

async function getCurrentRoundParticipants() {
  return consensusRead.getCurrentRoundParticipants();
}

async function getCurrentRoundDetails() {
  const result = await consensusRead.getCurrentRoundDetails();
  const participants = result[0];
  const values       = result[1];
  const faultyFlags  = result[2];

  return participants.map((addr, i) => ({
    address:     addr,
    value:       values[i].toString(),
    valueScaled: (Number(values[i]) / 100).toFixed(2),
    isFaulty:    faultyFlags[i]
  }));
}

async function getReading(roundId, sensorAddress) {
  const r = await consensusRead.getReading(roundId, sensorAddress);
  return {
    sensor: r.sensor,
    value: r.value.toString(),
    valueScaled: (Number(r.value) / 100).toFixed(2),
    timestamp: r.timestamp.toString(),
    isFaulty: r.isFaulty
  };
}

async function getConsensusStats() {
  const [
    currentRoundId,
    currentRoundStartTime,
    consensusWindow,
    minSensors,
    faultyThreshold,
    totalRounds,
    owner,
    registryAddress
  ] = await Promise.all([
    consensusRead.currentRoundId(),
    consensusRead.currentRoundStartTime(),
    consensusRead.consensusWindow(),
    consensusRead.minSensorsForConsensus(),
    consensusRead.faultyThresholdUnits(),
    consensusRead.totalRounds(),
    consensusRead.owner(),
    consensusRead.deviceRegistry()
  ]);

  const now = Math.floor(Date.now() / 1000);
  const elapsed = now - Number(currentRoundStartTime);
  const remaining = Math.max(0, Number(consensusWindow) - elapsed);

  return {
    contractAddress: process.env.SENSOR_CONSENSUS_ADDRESS,
    owner,
    currentRoundId: currentRoundId.toString(),
    currentRoundStartTime: currentRoundStartTime.toString(),
    consensusWindow: consensusWindow.toString(),
    roundTimeRemainingSeconds: remaining,
    minSensorsForConsensus: minSensors.toString(),
    faultyThresholdUnits: faultyThreshold.toString(),
    faultyThresholdScaled: (Number(faultyThreshold) / 100).toFixed(2),
    totalRounds: totalRounds.toString(),
    linkedRegistryAddress: registryAddress
  };
}

async function getAllRounds() {
  const currentRoundId = await consensusRead.currentRoundId();
  const total = Number(currentRoundId);
  const rounds = [];
  for (let i = 1; i <= total; i++) {
    try {
      const r = await consensusRead.getConsensusRound(i);
      rounds.push(formatRound(r));
    } catch {
      // round not yet finalised — skip
    }
  }
  return rounds;
}

// ============================================================
// EVENT HISTORY
// ============================================================

async function getEventHistory(fromBlock = 0) {
  const toBlock = "latest";

  const [
    readingEvents,
    faultyEvents,
    reachedEvents,
    rejectedEvents,
    newRoundEvents,
    registeredEvents,
    deactivatedEvents,
    reactivatedEvents,
    firmwareEvents
  ] = await Promise.all([
    consensusRead.queryFilter(consensusRead.filters.ReadingSubmitted(), fromBlock, toBlock),
    consensusRead.queryFilter(consensusRead.filters.FaultySensorDetected(), fromBlock, toBlock),
    consensusRead.queryFilter(consensusRead.filters.ConsensusReached(), fromBlock, toBlock),
    consensusRead.queryFilter(consensusRead.filters.ConsensusRejected(), fromBlock, toBlock),
    consensusRead.queryFilter(consensusRead.filters.NewRoundStarted(), fromBlock, toBlock),
    registryRead.queryFilter(registryRead.filters.DeviceRegistered(), fromBlock, toBlock),
    registryRead.queryFilter(registryRead.filters.DeviceDeactivated(), fromBlock, toBlock),
    registryRead.queryFilter(registryRead.filters.DeviceReactivated(), fromBlock, toBlock),
    registryRead.queryFilter(registryRead.filters.FirmwareUpdated(), fromBlock, toBlock)
  ]);

  return {
    readingsSubmitted: readingEvents.map(e => ({
      event: "ReadingSubmitted",
      roundId: e.args.roundId.toString(),
      sensor: e.args.sensor,
      value: e.args.value.toString(),
      valueScaled: (Number(e.args.value) / 100).toFixed(2),
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    })),
    faultySensors: faultyEvents.map(e => ({
      event: "FaultySensorDetected",
      roundId: e.args.roundId.toString(),
      sensor: e.args.sensor,
      submittedValue: e.args.submittedValue.toString(),
      submittedValueScaled: (Number(e.args.submittedValue) / 100).toFixed(2),
      disagreementScore: e.args.disagreementScore.toString(),
      scoreThreshold: e.args.scoreThreshold.toString(),
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    })),
    consensusReached: reachedEvents.map(e => ({
      event: "ConsensusReached",
      roundId: e.args.roundId.toString(),
      consensusValue: e.args.consensusValue.toString(),
      consensusValueScaled: (Number(e.args.consensusValue) / 100).toFixed(2),
      trustedParticipants: e.args.trustedParticipants.toString(),
      faultyCount: e.args.faultyCount.toString(),
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    })),
    consensusRejected: rejectedEvents.map(e => ({
      event: "ConsensusRejected",
      roundId: e.args.roundId.toString(),
      totalParticipants: e.args.totalParticipants.toString(),
      faultyCount: e.args.faultyCount.toString(),
      reason: e.args.reason,
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    })),
    newRounds: newRoundEvents.map(e => ({
      event: "NewRoundStarted",
      roundId: e.args.roundId.toString(),
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    })),
    devicesRegistered: registeredEvents.map(e => ({
      event: "DeviceRegistered",
      deviceAddress: e.args.deviceAddress,
      firmwareHash: e.args.firmwareHash,
      firmwareVersion: e.args.firmwareVersion.toString(),
      deviceType: e.args.deviceType,
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    })),
    devicesDeactivated: deactivatedEvents.map(e => ({
      event: "DeviceDeactivated",
      deviceAddress: e.args.deviceAddress,
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    })),
    devicesReactivated: reactivatedEvents.map(e => ({
      event: "DeviceReactivated",
      deviceAddress: e.args.deviceAddress,
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    })),
    firmwareUpdated: firmwareEvents.map(e => ({
      event: "FirmwareUpdated",
      deviceAddress: e.args.deviceAddress,
      oldFirmwareHash: e.args.oldFirmwareHash,
      newFirmwareHash: e.args.newFirmwareHash,
      newVersion: e.args.newVersion.toString(),
      timestamp: e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      txHash: e.transactionHash
    }))
  };
}

module.exports = {
  // DeviceRegistry write
  registerDevice,
  updateFirmware,
  deactivateDevice,
  reactivateDevice,
  // DeviceRegistry read
  getAllDevices,
  getDeviceInfo,
  verifyDevice,
  verifyFirmware,
  getDeviceFirmware,
  isDeviceRegistered,
  getRegistryStats,
  // SensorConsensus write
  submitReading,
  forceNewRound,
  forceConsensusCalculation,
  setFaultyThreshold,
  setMinSensorsForConsensus,
  setConsensusWindow,
  setDeviceRegistry,
  // SensorConsensus read
  getLatestConsensus,
  getLatestRound,
  getConsensusRound,
  getCurrentRoundParticipants,
  getCurrentRoundDetails,
  getReading,
  getConsensusStats,
  getAllRounds,
  // Events
  getEventHistory
};
