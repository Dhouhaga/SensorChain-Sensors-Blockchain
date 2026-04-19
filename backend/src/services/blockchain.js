// ============================================================
// blockchain.js — All ethers.js interaction in one place.
// Routes never touch ethers directly; they call this service.
//
// ENHANCED: Enriched transaction responses, block inspection,
//           full event traceability, and consensus explanation.
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
  let key = process.env[`SENSOR_PRIVATE_KEY_${sensorAddress}`];
  if (!key) {
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

// ============================================================
// FORMATTERS
// ============================================================

function formatDevice(d) {
  return {
    deviceAddress:    d.deviceAddress,
    firmwareHash:     d.firmwareHash,
    firmwareVersion:  d.firmwareVersion.toString(),
    deviceType:       d.deviceType,
    isActive:         d.isActive,
    registrationTime: d.registrationTime.toString(),
    lastUpdate:       d.lastUpdate.toString()
  };
}

async function formatRound(r, roundId) {
  // Helper function to convert array-like objects to real arrays
  function toArray(arr) {
    if (!arr) return [];
    if (Array.isArray(arr)) return arr;
    if (typeof arr === 'object' && arr.length !== undefined) {
      const result = [];
      for (let i = 0; i < arr.length; i++) {
        if (arr[i] !== undefined && arr[i] !== null) {
          result.push(arr[i]);
        }
      }
      return result;
    }
    return [];
  }

  // Extract arrays from the struct
  let participants = toArray(r.participants);
  let values = toArray(r.values);
  const disagreementScores = toArray(r.disagreementScores);
  const faultyFlags = toArray(r.faultyFlags);

  // CRITICAL FIX: If values are missing but participants exist, fetch from roundReadings
  if (values.length === 0 && participants.length > 0 && roundId) {
    console.log(`Values missing for round ${roundId}, fetching from getReading...`);
    const fetchedValues = [];
    for (const sensor of participants) {
      try {
        const reading = await consensusRead.getReading(roundId, sensor);
        fetchedValues.push(reading.value.toString());
      } catch (err) {
        console.error(`Failed to get reading for ${sensor}:`, err.message);
        fetchedValues.push("0");
      }
    }
    values = fetchedValues;
  }

  console.log('formatRound debug:', {
    roundId,
    participantsCount: participants.length,
    valuesCount: values.length,
    scoresCount: disagreementScores.length,
    flagsCount: faultyFlags.length,
  });

  // Convert BigNumber values to strings
  const valuesStr = values.map(v => v ? (v.toString ? v.toString() : String(v)) : "0");
  const scoresStr = disagreementScores.map(s => s ? (s.toString ? s.toString() : String(s)) : "0");

  return {
    roundId:              r.roundId.toString(),
    timestamp:            r.timestamp.toString(),
    consensusValue:       r.consensusValue.toString(),
    consensusValueScaled: (Number(r.consensusValue) / 100).toFixed(2),
    totalParticipants:    r.totalParticipants.toString(),
    trustedParticipants:  r.trustedParticipants.toString(),
    faultyCount:          r.faultyCount.toString(),
    consensusReached:     r.consensusReached,
    participants:         participants,
    values:               valuesStr,
    valuesScaled:         valuesStr.map(v => (Number(v) / 100).toFixed(2)),
    disagreementScores:   scoresStr,
    faultyFlags:          faultyFlags,
    sensors: participants.map((addr, i) => ({
      address:           addr,
      value:             valuesStr[i] || "0",
      valueScaled:       valuesStr[i] ? (Number(valuesStr[i]) / 100).toFixed(2) : "0.00",
      disagreementScore: scoresStr[i] || "0",
      isFaulty:          faultyFlags[i] || false
    }))
  };
}

/**
 * Enriches a raw transaction receipt with block metadata, gas price,
 * sender/receiver, log count, and human timestamp.
 *
 * @param {object} receipt  - ethers TransactionReceipt
 * @param {object} [tx]     - ethers Transaction (optional, fetched if omitted)
 * @param {object} [block]  - ethers Block (optional, fetched if omitted)
 */
async function formatEnrichedReceipt(receipt, tx, block) {
  if (!tx)    tx    = await provider.getTransaction(receipt.transactionHash);
  if (!block) block = await provider.getBlock(receipt.blockNumber);

  return {
    // Core identifiers
    transactionHash: receipt.transactionHash,
    blockNumber:     receipt.blockNumber,
    blockHash:       receipt.blockHash,

    // Parties
    from: tx.from,
    to:   tx.to,

    // Gas
    gasUsed:    receipt.gasUsed.toString(),
    gasPrice:   tx.gasPrice  ? tx.gasPrice.toString()  : "0",
    gasCostWei: tx.gasPrice  ? tx.gasPrice.mul(receipt.gasUsed).toString() : "0",

    // Status
    status:        receipt.status === 1 ? "success" : "failed",
    confirmations: receipt.confirmations,

    // Block context
    blockTimestamp:        block.timestamp.toString(),
    blockTimestampISO:     new Date(block.timestamp * 1000).toISOString(),
    blockTransactionCount: block.transactions.length,

    // Logs
    logsCount: receipt.logs.length
  };
}

/**
 * Decode logs from a receipt using both contract interfaces.
 */
function decodeLogs(receipt) {
  const registryIface  = new ethers.utils.Interface(DEVICE_REGISTRY_ABI);
  const consensusIface = new ethers.utils.Interface(SENSOR_CONSENSUS_ABI);

  return receipt.logs.map(log => {
    let decoded = null;
    for (const iface of [consensusIface, registryIface]) {
      try {
        const parsed = iface.parseLog(log);
        decoded = {
          eventName: parsed.name,
          args: Object.fromEntries(
            parsed.eventFragment.inputs.map((inp, i) => [
              inp.name,
              parsed.args[i]?.toString?.() ?? parsed.args[i]
            ])
          )
        };
        break;
      } catch { /* try next */ }
    }
    return {
      logIndex:        log.logIndex,
      address:         log.address,
      topics:          log.topics,
      data:            log.data,
      decoded
    };
  });
}

// ============================================================
// BLOCKCHAIN INSPECTION
// ============================================================

/**
 * Returns latest block summary.
 */
async function getLatestBlock() {
  const block = await provider.getBlock("latest");
  return {
    blockNumber:      block.number,
    blockHash:        block.hash,
    parentHash:       block.parentHash,
    timestamp:        block.timestamp.toString(),
    timestampISO:     new Date(block.timestamp * 1000).toISOString(),
    transactionCount: block.transactions.length,
    gasUsed:          block.gasUsed.toString(),
    gasLimit:         block.gasLimit.toString(),
    miner:            block.miner
  };
}

/**
 * Returns a full block with all its transactions.
 */
async function getBlockByNumber(number) {
  const block = await provider.getBlockWithTransactions(number);
  if (!block) throw new Error(`Block ${number} not found`);
  return {
    blockNumber:      block.number,
    blockHash:        block.hash,
    parentHash:       block.parentHash,
    timestamp:        block.timestamp.toString(),
    timestampISO:     new Date(block.timestamp * 1000).toISOString(),
    gasUsed:          block.gasUsed.toString(),
    gasLimit:         block.gasLimit.toString(),
    miner:            block.miner,
    transactionCount: block.transactions.length,
    transactions: block.transactions.map(tx => ({
      hash:     tx.hash,
      from:     tx.from,
      to:       tx.to,
      value:    tx.value.toString(),
      gasLimit: tx.gasLimit.toString(),
      gasPrice: tx.gasPrice?.toString() ?? "0",
      nonce:    tx.nonce,
      data:     tx.data.length > 66 ? tx.data.slice(0, 66) + "…" : tx.data
    }))
  };
}

/**
 * Returns full transaction info: tx + receipt + decoded logs.
 */
async function getTransactionDetails(txHash) {
  const [tx, receipt] = await Promise.all([
    provider.getTransaction(txHash),
    provider.getTransactionReceipt(txHash)
  ]);
  if (!tx) throw new Error(`Transaction ${txHash} not found`);

  const block       = await provider.getBlock(receipt.blockNumber);
  const enriched    = await formatEnrichedReceipt(receipt, tx, block);
  const decodedLogs = decodeLogs(receipt);

  return {
    ...enriched,
    nonce:    tx.nonce,
    value:    tx.value.toString(),
    input:    tx.data,
    logs:     decodedLogs
  };
}

/**
 * Returns network/RPC info.
 */
async function getNetworkInfo() {
  const [network, blockNumber, gasPrice] = await Promise.all([
    provider.getNetwork(),
    provider.getBlockNumber(),
    provider.getGasPrice()
  ]);
  return {
    chainId:         network.chainId,
    networkName:     network.name || "ganache",
    rpcUrl:          process.env.RPC_URL,
    latestBlock:     blockNumber,
    gasPrice:        gasPrice.toString(),
    gasPriceGwei:    ethers.utils.formatUnits(gasPrice, "gwei"),
    contractAddresses: {
      deviceRegistry:  process.env.DEVICE_REGISTRY_ADDRESS,
      sensorConsensus: process.env.SENSOR_CONSENSUS_ADDRESS
    }
  };
}

// ============================================================
// DEVICE REGISTRY — WRITE
// ============================================================

async function registerDevice(deviceAddress, firmwareHash, firmwareVersion, deviceType) {
  const tx      = await registryWrite.registerDevice(
    deviceAddress, firmwareHash, firmwareVersion, deviceType,
    { gasLimit: GAS_LIMIT }
  );
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

async function updateFirmware(deviceAddress, newFirmwareHash, newVersion) {
  const tx      = await registryWrite.updateFirmware(
    deviceAddress, newFirmwareHash, newVersion,
    { gasLimit: GAS_LIMIT }
  );
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

async function deactivateDevice(deviceAddress) {
  const tx      = await registryWrite.deactivateDevice(deviceAddress, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

async function reactivateDevice(deviceAddress) {
  const tx      = await registryWrite.reactivateDevice(deviceAddress, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

// ============================================================
// DEVICE REGISTRY — READ
// ============================================================

async function getAllDevices() {
  const addresses = await registryRead.getAllDevices();
  const devices   = await Promise.all(
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
    firmwareHash:    result.firmwareHash,
    firmwareVersion: result.firmwareVersion.toString(),
    lastUpdate:      result.lastUpdate.toString()
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
    contractOwner:   owner,
    totalDevices:    total.toString(),
    contractAddress: process.env.DEVICE_REGISTRY_ADDRESS
  };
}

// ============================================================
// SENSOR CONSENSUS — WRITE (sensor)
// ============================================================

async function submitReading(sensorAddress, value, firmwareHash) {
  const wallet             = getSensorWallet(sensorAddress);
  const consensusWithSensor = consensusRead.connect(wallet);

  const timestamp  = Math.floor(Date.now() / 1000);
  const messageHash = ethers.utils.solidityKeccak256(
    ["int256", "uint256"],
    [value, timestamp]
  );
  const signature = await wallet.signMessage(ethers.utils.arrayify(messageHash));

  const tx      = await consensusWithSensor.submitReading(
    value, timestamp, firmwareHash, signature,
    { gasLimit: GAS_LIMIT }
  );
  const receipt = await tx.wait();
  const enriched = await formatEnrichedReceipt(receipt);

  return {
    ...enriched,
    sensorAddress,
    value:       value.toString(),
    valueScaled: (Number(value) / 100).toFixed(2),
    timestamp,
    firmwareHash
  };
}

// ============================================================
// SENSOR CONSENSUS — WRITE (admin)
// ============================================================

async function forceNewRound() {
  const tx      = await consensusWrite.forceNewRound({ gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

async function forceConsensusCalculation() {
  const tx      = await consensusWrite.forceConsensusCalculation({ gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

async function setFaultyThreshold(threshold) {
  const tx      = await consensusWrite.setFaultyThreshold(threshold, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

async function setMinSensorsForConsensus(minSensors) {
  const tx      = await consensusWrite.setMinSensorsForConsensus(minSensors, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

async function setConsensusWindow(windowSeconds) {
  const tx      = await consensusWrite.setConsensusWindow(windowSeconds, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

async function setDeviceRegistry(newRegistryAddress) {
  const tx      = await consensusWrite.setDeviceRegistry(newRegistryAddress, { gasLimit: GAS_LIMIT });
  const receipt = await tx.wait();
  return formatEnrichedReceipt(receipt);
}

// ============================================================
// SENSOR CONSENSUS — READ
// ============================================================

async function getLatestConsensus() {
  const result = await consensusRead.getLatestConsensus();
  return {
    consensusValue:       result.consensusValue.toString(),
    consensusValueScaled: (Number(result.consensusValue) / 100).toFixed(2),
    trustedCount:         result.trustedCount.toString(),
    faultyCount:          result.faultyCount.toString(),
    timestamp:            result.timestamp.toString()
  };
}

async function getLatestRound() {
  try {
    const currentRoundId = await consensusRead.currentRoundId();
    const total = Number(currentRoundId);
    for (let rid = total; rid >= 1; rid--) {
      try {
        const r = await consensusRead.getConsensusRound(rid);
        if (r.timestamp && r.timestamp.toString() !== "0") {
          return {
            roundId:              r.roundId.toString(),
            consensusReached:     r.consensusReached,
            consensusValue:       r.consensusValue.toString(),
            consensusValueScaled: (Number(r.consensusValue) / 100).toFixed(2),
            totalParticipants:    r.totalParticipants.toString(),
            trustedParticipants:  r.trustedParticipants.toString(),
            faultyCount:          r.faultyCount.toString(),
            timestamp:            r.timestamp.toString(),
            noRoundsYet:          false
          };
        }
      } catch { continue; }
    }
    return {
      roundId: "0", consensusReached: false, consensusValue: "0",
      consensusValueScaled: "0.00", totalParticipants: "0",
      trustedParticipants: "0", faultyCount: "0", timestamp: "0",
      noRoundsYet: true
    };
  } catch {
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
  return await formatRound(r, roundId);
}

async function getCurrentRoundParticipants() {
  return consensusRead.getCurrentRoundParticipants();
}

async function getCurrentRoundDetails() {
  const result      = await consensusRead.getCurrentRoundDetails();
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
    sensor:      r.sensor,
    value:       r.value.toString(),
    valueScaled: (Number(r.value) / 100).toFixed(2),
    timestamp:   r.timestamp.toString(),
    isFaulty:    r.isFaulty
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

  const now       = Math.floor(Date.now() / 1000);
  const elapsed   = now - Number(currentRoundStartTime);
  const remaining = Math.max(0, Number(consensusWindow) - elapsed);

  return {
    contractAddress:          process.env.SENSOR_CONSENSUS_ADDRESS,
    owner,
    currentRoundId:           currentRoundId.toString(),
    currentRoundStartTime:    currentRoundStartTime.toString(),
    consensusWindow:          consensusWindow.toString(),
    roundTimeRemainingSeconds: remaining,
    minSensorsForConsensus:   minSensors.toString(),
    faultyThresholdUnits:     faultyThreshold.toString(),
    faultyThresholdScaled:    (Number(faultyThreshold) / 100).toFixed(2),
    totalRounds:              totalRounds.toString(),
    linkedRegistryAddress:    registryAddress
  };
}

async function getAllRounds() {
  const currentRoundId = await consensusRead.currentRoundId();
  const total  = Number(currentRoundId);
  const rounds = [];
  for (let i = 1; i <= total; i++) {
    try {
      const r = await consensusRead.getConsensusRound(i);
      rounds.push(await formatRound(r, i));
    } catch { /* not yet finalised */ }
  }
  return rounds;
}

// ============================================================
// CONSENSUS EXPLANATION
// ============================================================

/**
 * Reconstruct the full pairwise-disagreement algorithm in JS
 * so the explanation is computed deterministically without
 * extra contract calls beyond what we already have.
 *
 * Returns a human-readable breakdown that mirrors what the
 * Solidity _calculateConsensus() function actually did.
 */
async function explainConsensusRound(roundId) {
  // 1. Fetch the stored round (already includes scores + flags)
  const raw = await consensusRead.getConsensusRound(roundId);
  const r = await formatRound(raw, roundId);

  console.log('Explained round data:', {
    roundId: r.roundId,
    participantCount: r.participants.length,
    valuesCount: r.values.length,
    firstValue: r.values[0],
    firstValueScaled: r.valuesScaled[0]
  });

  const count = r.participants.length;
  const faultyThreshRaw = Number(await consensusRead.faultyThresholdUnits());
  const scoreThreshold = faultyThreshRaw * (count - 1);

  // 2. Re-derive pairwise pairs for the explanation
  const pairwiseComparisons = [];
  for (let i = 0; i < count - 1; i++) {
    for (let j = i + 1; j < count; j++) {
      const vi = Number(r.values[i]);
      const vj = Number(r.values[j]);
      const diff = Math.abs(vi - vj);
      pairwiseComparisons.push({
        sensorA: r.participants[i],
        sensorAValue: r.valuesScaled[i],
        sensorB: r.participants[j],
        sensorBValue: r.valuesScaled[j],
        differenceRaw: diff.toString(),
        differenceScaled: (diff / 100).toFixed(2)
      });
    }
  }

  // 3. Per-sensor analysis
  const sensorAnalysis = r.participants.map((addr, i) => {
    const scoreRaw = Number(r.disagreementScores[i]);
    const isFaulty = r.faultyFlags[i];
    const excess = isFaulty ? scoreRaw - scoreThreshold : 0;
    return {
      address: addr,
      submittedValue: r.valuesScaled[i],
      submittedValueRaw: r.values[i],
      disagreementScore: scoreRaw.toString(),
      disagreementScoreScaled: (scoreRaw / 100).toFixed(2),
      scoreThreshold: scoreThreshold.toString(),
      scoreThresholdScaled: (scoreThreshold / 100).toFixed(2),
      isFaulty,
      verdict: isFaulty
        ? `FAULTY — score ${(scoreRaw/100).toFixed(2)} exceeds threshold ${(scoreThreshold/100).toFixed(2)} by ${(excess/100).toFixed(2)}`
        : `TRUSTED — score ${(scoreRaw/100).toFixed(2)} within threshold ${(scoreThreshold/100).toFixed(2)}`
    };
  });

  // 4. Trusted average reconstruction
  const trustedSensors = sensorAnalysis.filter(s => !s.isFaulty);
  const trustedSum = trustedSensors.reduce((acc, s) => acc + Number(s.submittedValueRaw), 0);
  const trustedAvgRaw = trustedSensors.length > 0 ? Math.trunc(trustedSum / trustedSensors.length) : 0;

  // 5. Fetch the consensus event for txHash + blockNumber traceability
  const toBlock = "latest";
  let txTrace = null;
  try {
    const eventFilter = r.consensusReached
      ? consensusRead.filters.ConsensusReached(roundId)
      : consensusRead.filters.ConsensusRejected(roundId);

    const events = await consensusRead.queryFilter(eventFilter, 0, toBlock);
    if (events.length > 0) {
      const ev = events[0];
      const block = await provider.getBlock(ev.blockNumber);
      txTrace = {
        transactionHash: ev.transactionHash,
        blockNumber: ev.blockNumber,
        blockHash: ev.blockHash,
        blockTimestamp: block.timestamp.toString(),
        blockTimestampISO: new Date(block.timestamp * 1000).toISOString(),
        eventName: r.consensusReached ? "ConsensusReached" : "ConsensusRejected"
      };
    }
  } catch { /* event lookup is best-effort */ }

  // 6. Assemble explanation
  return {
    roundId: r.roundId,

    inputs: {
      sensorCount: count,
      sensors: r.participants.map((addr, i) => ({
        address: addr,
        valueRaw: r.values[i],
        valueScaled: r.valuesScaled[i]
      }))
    },

    step1_pairwiseComparisons: {
      description: "For every pair (i,j): diff = |value_i - value_j|. Both sensors accumulate diff in their disagreement score.",
      totalPairs: pairwiseComparisons.length,
      pairs: pairwiseComparisons
    },

    step2_faultDetection: {
      description: "A sensor is FAULTY if its total disagreement score exceeds faultyThresholdUnits × (count−1).",
      faultyThresholdUnits: faultyThreshRaw.toString(),
      faultyThresholdScaled: (faultyThreshRaw / 100).toFixed(2),
      scoreThresholdRaw: scoreThreshold.toString(),
      scoreThresholdScaled: (scoreThreshold / 100).toFixed(2),
      formula: `threshold = ${faultyThreshRaw} × (${count}−1) = ${scoreThreshold}`,
      sensors: sensorAnalysis
    },

    step3_safetyCheck: {
      description: "Round is REJECTED if faultyCount > totalCount / 2 (majority faulty).",
      totalSensors: count,
      faultySensors: Number(r.faultyCount),
      majorityLimit: Math.floor(count / 2),
      roundRejected: !r.consensusReached && Number(r.faultyCount) > 0,
      verdict: Number(r.faultyCount) > Math.floor(count / 2)
        ? `REJECTED — ${r.faultyCount} faulty sensors exceeds majority limit of ${Math.floor(count/2)}`
        : `PASSED — ${r.faultyCount} faulty sensors within majority limit of ${Math.floor(count/2)}`
    },

    step4_trustedAverage: {
      description: "Consensus value = average of non-faulty sensor readings only.",
      trustedSensors: trustedSensors.map(s => ({
        address: s.address,
        valueRaw: s.submittedValueRaw,
        valueScaled: s.submittedValue
      })),
      trustedCount: trustedSensors.length,
      trustedSumRaw: trustedSum.toString(),
      computedAverageRaw: trustedAvgRaw.toString(),
      computedAverageScaled: (trustedAvgRaw / 100).toFixed(2),
      storedConsensusRaw: r.consensusValue,
      storedConsensusScaled: r.consensusValueScaled
    },

    result: {
      consensusReached: r.consensusReached,
      consensusValue: r.consensusValue,
      consensusValueScaled: r.consensusValueScaled,
      trustedParticipants: r.trustedParticipants,
      faultyCount: r.faultyCount,
      totalParticipants: r.totalParticipants
    },

    blockchainTrace: txTrace
  };
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
    consensusRead.queryFilter(consensusRead.filters.ReadingSubmitted(),    fromBlock, toBlock),
    consensusRead.queryFilter(consensusRead.filters.FaultySensorDetected(), fromBlock, toBlock),
    consensusRead.queryFilter(consensusRead.filters.ConsensusReached(),    fromBlock, toBlock),
    consensusRead.queryFilter(consensusRead.filters.ConsensusRejected(),   fromBlock, toBlock),
    consensusRead.queryFilter(consensusRead.filters.NewRoundStarted(),     fromBlock, toBlock),
    registryRead.queryFilter(registryRead.filters.DeviceRegistered(),      fromBlock, toBlock),
    registryRead.queryFilter(registryRead.filters.DeviceDeactivated(),     fromBlock, toBlock),
    registryRead.queryFilter(registryRead.filters.DeviceReactivated(),     fromBlock, toBlock),
    registryRead.queryFilter(registryRead.filters.FirmwareUpdated(),       fromBlock, toBlock)
  ]);

  // Helper: enrich an event with block timestamp
  const blockCache = {};
  async function blockTimestamp(blockNumber) {
    if (!blockCache[blockNumber]) {
      const b = await provider.getBlock(blockNumber);
      blockCache[blockNumber] = b.timestamp;
    }
    return blockCache[blockNumber];
  }

  // We enrich only the most meaningful events to avoid too many RPC calls
  const enrichedReached = await Promise.all(reachedEvents.map(async e => {
    const ts = await blockTimestamp(e.blockNumber);
    return {
      event:               "ConsensusReached",
      roundId:             e.args.roundId.toString(),
      consensusValue:      e.args.consensusValue.toString(),
      consensusValueScaled:(Number(e.args.consensusValue) / 100).toFixed(2),
      trustedParticipants: e.args.trustedParticipants.toString(),
      faultyCount:         e.args.faultyCount.toString(),
      timestamp:           e.args.timestamp.toString(),
      blockNumber:         e.blockNumber,
      blockHash:           e.blockHash,
      blockTimestamp:      ts.toString(),
      blockTimestampISO:   new Date(ts * 1000).toISOString(),
      txHash:              e.transactionHash
    };
  }));

  const enrichedRejected = await Promise.all(rejectedEvents.map(async e => {
    const ts = await blockTimestamp(e.blockNumber);
    return {
      event:             "ConsensusRejected",
      roundId:           e.args.roundId.toString(),
      totalParticipants: e.args.totalParticipants.toString(),
      faultyCount:       e.args.faultyCount.toString(),
      reason:            e.args.reason,
      timestamp:         e.args.timestamp.toString(),
      blockNumber:       e.blockNumber,
      blockHash:         e.blockHash,
      blockTimestamp:    ts.toString(),
      blockTimestampISO: new Date(ts * 1000).toISOString(),
      txHash:            e.transactionHash
    };
  }));

  return {
    readingsSubmitted: readingEvents.map(e => ({
      event:       "ReadingSubmitted",
      roundId:     e.args.roundId.toString(),
      sensor:      e.args.sensor,
      value:       e.args.value.toString(),
      valueScaled: (Number(e.args.value) / 100).toFixed(2),
      timestamp:   e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      blockHash:   e.blockHash,
      txHash:      e.transactionHash
    })),

    faultySensors: faultyEvents.map(e => ({
      event:               "FaultySensorDetected",
      roundId:             e.args.roundId.toString(),
      sensor:              e.args.sensor,
      submittedValue:      e.args.submittedValue.toString(),
      submittedValueScaled:(Number(e.args.submittedValue) / 100).toFixed(2),
      disagreementScore:   e.args.disagreementScore.toString(),
      scoreThreshold:      e.args.scoreThreshold.toString(),
      timestamp:           e.args.timestamp.toString(),
      blockNumber:         e.blockNumber,
      blockHash:           e.blockHash,
      txHash:              e.transactionHash
    })),

    consensusReached:  enrichedReached,
    consensusRejected: enrichedRejected,

    newRounds: newRoundEvents.map(e => ({
      event:       "NewRoundStarted",
      roundId:     e.args.roundId.toString(),
      timestamp:   e.args.timestamp.toString(),
      blockNumber: e.blockNumber,
      blockHash:   e.blockHash,
      txHash:      e.transactionHash
    })),

    devicesRegistered: registeredEvents.map(e => ({
      event:           "DeviceRegistered",
      deviceAddress:   e.args.deviceAddress,
      firmwareHash:    e.args.firmwareHash,
      firmwareVersion: e.args.firmwareVersion.toString(),
      deviceType:      e.args.deviceType,
      timestamp:       e.args.timestamp.toString(),
      blockNumber:     e.blockNumber,
      blockHash:       e.blockHash,
      txHash:          e.transactionHash
    })),

    devicesDeactivated: deactivatedEvents.map(e => ({
      event:         "DeviceDeactivated",
      deviceAddress: e.args.deviceAddress,
      timestamp:     e.args.timestamp.toString(),
      blockNumber:   e.blockNumber,
      blockHash:     e.blockHash,
      txHash:        e.transactionHash
    })),

    devicesReactivated: reactivatedEvents.map(e => ({
      event:         "DeviceReactivated",
      deviceAddress: e.args.deviceAddress,
      timestamp:     e.args.timestamp.toString(),
      blockNumber:   e.blockNumber,
      blockHash:     e.blockHash,
      txHash:        e.transactionHash
    })),

    firmwareUpdated: firmwareEvents.map(e => ({
      event:           "FirmwareUpdated",
      deviceAddress:   e.args.deviceAddress,
      oldFirmwareHash: e.args.oldFirmwareHash,
      newFirmwareHash: e.args.newFirmwareHash,
      newVersion:      e.args.newVersion.toString(),
      timestamp:       e.args.timestamp.toString(),
      blockNumber:     e.blockNumber,
      blockHash:       e.blockHash,
      txHash:          e.transactionHash
    }))
  };
}

// ============================================================
// EXPORTS
// ============================================================

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
  // Consensus explanation
  explainConsensusRound,
  // Blockchain inspection
  getLatestBlock,
  getBlockByNumber,
  getTransactionDetails,
  getNetworkInfo,
  // Events
  getEventHistory
};