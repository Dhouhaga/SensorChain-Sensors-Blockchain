// blockchain.js  All ethers.js interaction in one place.
// Routes never touch ethers directly; they call this service.
//
// ENHANCED: Enriched transaction responses, block inspection,
//           full event traceability, consensus explanation,
//           and proper blockchain revert error handling.

const { ethers } = require("ethers");
const { DEVICE_REGISTRY_ABI, SENSOR_CONSENSUS_ABI } = require("../config/abis");

const GAS_LIMIT = 1000000;

//  Provider & signers 

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

//  Helper: get a sensor wallet from env 

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
  try {
    if (!tx && receipt) {
      try {
        tx = await provider.getTransaction(receipt.transactionHash);
      } catch (err) {
        console.warn("Could not fetch transaction:", err.message);
      }
    }
    if (!block && receipt && receipt.blockNumber) {
      try {
        block = await provider.getBlock(receipt.blockNumber);
      } catch (err) {
        console.warn("Could not fetch block:", err.message);
      }
    }

    return {
      transactionHash: receipt?.transactionHash || "unknown",
      blockNumber:     receipt?.blockNumber || 0,
      blockHash:       receipt?.blockHash || "unknown",

      from: tx?.from || "unknown",
      to:   tx?.to || "unknown",

      gasUsed:    receipt?.gasUsed?.toString() || "0",
      gasPrice:   tx?.gasPrice?.toString() || "0",
      gasCostWei: tx?.gasPrice && receipt?.gasUsed 
        ? tx.gasPrice.mul(receipt.gasUsed).toString() 
        : "0",

      status:        receipt?.status === 1 ? "success" : receipt?.status === 0 ? "failed" : "unknown",
      confirmations: receipt?.confirmations || 0,

      blockTimestamp:        block?.timestamp?.toString() || "0",
      blockTimestampISO:     block?.timestamp ? new Date(block.timestamp * 1000).toISOString() : null,
      blockTransactionCount: block?.transactions?.length || 0,

      logsCount: receipt?.logs?.length || 0
    };
  } catch (err) {
    console.error("Error formatting receipt:", err);
    return {
      transactionHash: receipt?.transactionHash || "unknown",
      blockNumber: receipt?.blockNumber || 0,
      blockHash: receipt?.blockHash || "unknown",
      from: "unknown",
      to: "unknown",
      gasUsed: "0",
      gasPrice: "0",
      gasCostWei: "0",
      status: "unknown",
      confirmations: 0,
      blockTimestamp: "0",
      blockTimestampISO: null,
      blockTransactionCount: 0,
      logsCount: 0
    };
  }
}

/**
 * Format a blockchain error into a user-friendly response
 */
/**
 * Format a blockchain error into a user-friendly response
 */
function formatBlockchainError(error, txHash = null) {
  let errorReason = null;
  let errorType = "unknown";
  
  if (error.reason) {
    errorReason = error.reason;
  } 
  // Check for nested error.data (from ethers)
  else if (error.error && error.error.data && error.error.data.reason) {
    errorReason = error.error.data.reason;
  }
  else if (error.error && error.error.data && error.error.data.message) {
    const msg = error.error.data.message;
    const revertMatch = msg.match(/revert (.*?)(?:"|$)/);
    errorReason = revertMatch ? revertMatch[1] : msg;
  }
  else if (error.data && error.data.reason) {
    errorReason = error.data.reason;
  }
  else if (error.data && error.data.message) {
    const msg = error.data.message;
    const revertMatch = msg.match(/revert (.*?)(?:"|$)/);
    errorReason = revertMatch ? revertMatch[1] : msg;
  }
  else if (error.error && error.error.message) {
    const msg = error.error.message;
    const revertMatch = msg.match(/revert (.*?)(?:"|$)/);
    errorReason = revertMatch ? revertMatch[1] : msg;
  }
  else if (error.message) {
    const revertMatch = error.message.match(/revert (.*?)(?:"|$)/);
    errorReason = revertMatch ? revertMatch[1] : null;
  }
  
  // If still no reason, try to get from the original error's nested structure
  if (!errorReason && error.originalError) {
    if (error.originalError.data && error.originalError.data.reason) {
      errorReason = error.originalError.data.reason;
    } else if (error.originalError.reason) {
      errorReason = error.originalError.reason;
    }
  }
  
  console.log("Extracted error reason:", errorReason);
  
  const errorMessages = {
    "Device not registered or inactive": {
      message: " Sensor not registered or deactivated",
      action: "Please select an active sensor or contact administrator to reactivate this sensor.",
      type: "authentication"
    },
    "Security Alert: Firmware hash mismatch": {
      message: "!! Security Alert: Firmware hash mismatch",
      action: "The device is not running authorized firmware. Please update the firmware hash in DeviceRegistry.",
      type: "security"
    },
    "Security Alert: Invalid cryptographic signature": {
      message: "!! Invalid signature - Authentication failed",
      action: "The reading was not properly signed by the sensor. Check sensor private key configuration.",
      type: "security"
    },
    "Device already submitted for this round": {
      message: " Duplicate submission",
      action: "This sensor has already submitted a reading for the current round. Wait for next round.",
      type: "duplicate"
    },
    "Device not found": {
      message: " Device not found in registry",
      action: "This sensor address is not registered. Please register the device first.",
      type: "authentication"
    },
    "Only contract owner can call this function": {
      message: "!! Admin only operation",
      action: "This operation requires contract owner privileges.",
      type: "permission"
    }
  };
  
  const knownError = errorMessages[errorReason];
  if (knownError) {
    return {
      success: false,
      error: knownError.message,
      action: knownError.action,
      type: knownError.type,
      details: {
        reason: errorReason,
        transactionHash: txHash,
        blockchainError: true
      }
    };
  }
  
  return {
    success: false,
    error: errorReason ? ` Blockchain error: ${errorReason}` : " Blockchain transaction failed",
    action: errorReason 
      ? `Reason: ${errorReason}. Please check sensor status and try again.` 
      : "Please check sensor status and try again.",
    type: "unknown",
    details: {
      reason: errorReason || error.message || "Unknown error",
      transactionHash: txHash,
      blockchainError: true
    }
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
      data:     tx.data.length > 66 ? tx.data.slice(0, 66) + "" : tx.data
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

// DEVICE REGISTRY  WRITE

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

// DEVICE REGISTRY  READ

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

// SENSOR CONSENSUS  WRITE (sensor)
// SENSOR CONSENSUS  WRITE (sensor)

async function submitReading(sensorAddress, value, firmwareHash) {
  let tx = null;
  
  try {
    const wallet = getSensorWallet(sensorAddress);
    const consensusWithSensor = consensusRead.connect(wallet);

    const timestamp = Math.floor(Date.now() / 1000);
    const messageHash = ethers.utils.solidityKeccak256(
      ["int256", "uint256"],
      [value, timestamp]
    );
    const signature = await wallet.signMessage(ethers.utils.arrayify(messageHash));

    tx = await consensusWithSensor.submitReading(
      value, timestamp, firmwareHash, signature,
      { gasLimit: GAS_LIMIT }
    );
    
    const receipt = await tx.wait();
    
    const enriched = await formatEnrichedReceipt(receipt);
    
    return {
      ...enriched,
      sensorAddress,
      value: value.toString(),
      valueScaled: (Number(value) / 100).toFixed(2),
      timestamp,
      firmwareHash
    };
    
  } catch (error) {
    console.error("Blockchain submit error:", error);
    
    const txHash = tx ? tx.hash : (error.transactionHash || (error.transaction ? error.transaction.hash : null));
    
    let errorReason = null;
    
    // The actual revert reason is deep in error.error.data.reason
    if (error.error && error.error.data && error.error.data.reason) {
      errorReason = error.error.data.reason;
    } 
    else if (error.data && error.data.reason) {
      errorReason = error.data.reason;
    }
    else if (error.reason && error.reason !== "processing response error") {
      errorReason = error.reason;
    }
    else if (error.error && error.error.message) {
      const msg = error.error.message;
      const revertMatch = msg.match(/revert (.*?)(?:"|$)/);
      if (revertMatch) errorReason = revertMatch[1];
    }
    else if (error.message) {
      const revertMatch = error.message.match(/revert (.*?)(?:"|$)/);
      if (revertMatch) errorReason = revertMatch[1];
    }
    
    console.log("Extracted error reason:", errorReason);
    
    let userMessage = " Blockchain transaction failed";
    let userAction = "Please check sensor status and try again.";
    let errorType = "unknown";
    
    switch(errorReason) {
      case "Device not registered or inactive":
        userMessage = " Sensor not registered or deactivated";
        userAction = "Please select an active sensor or contact administrator to reactivate this sensor.";
        errorType = "authentication";
        break;
      case "Security Alert: Firmware hash mismatch":
        userMessage = "!! Security Alert: Firmware hash mismatch";
        userAction = "The device is not running authorized firmware. Please update the firmware hash in DeviceRegistry.";
        errorType = "security";
        break;
      case "Security Alert: Invalid cryptographic signature":
        userMessage = "!! Invalid signature - Authentication failed";
        userAction = "The reading was not properly signed by the sensor. Check sensor private key configuration.";
        errorType = "security";
        break;
      case "Device already submitted for this round":
        userMessage = " Duplicate submission";
        userAction = "This sensor has already submitted a reading for the current round. Wait for next round.";
        errorType = "duplicate";
        break;
      case "Device not found":
        userMessage = " Device not found in registry";
        userAction = "This sensor address is not registered. Please register the device first.";
        errorType = "authentication";
        break;
    }
    
    const formattedError = {
      success: false,
      error: userMessage,
      action: userAction,
      type: errorType,
      details: {
        reason: errorReason,
        transactionHash: txHash,
        blockchainError: true
      }
    };
    
    const throwError = new Error(userMessage);
    throwError.formattedResponse = formattedError;
    throwError.transactionHash = txHash;
    throwError.originalError = error;
    throw throwError;
  }
}

// SENSOR CONSENSUS  WRITE (admin)

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

// SENSOR CONSENSUS  READ

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


/**
 * Reconstruct the full pairwise-disagreement algorithm in JS
 * so the explanation is computed deterministically without
 * extra contract calls beyond what we already have.
 *
 * Returns a human-readable breakdown that mirrors what the
 * Solidity _calculateConsensus() function actually did.
 */
async function explainConsensusRound(roundId) {
  let participants = [];
  let values = [];
  let faultyFlags = [];
  let disagreementScores = [];
  let consensusReached = false;
  let consensusValue = "0";
  let trustedParticipants = "0";
  let faultyCount = "0";
  let totalParticipants = "0";
  let timestamp = "0";
  
  // FIRST: Try to get participants from roundParticipants mapping via events
  try {
    const events = await consensusRead.queryFilter(
      consensusRead.filters.ReadingSubmitted(roundId),
      0,
      'latest'
    );
    
    const uniqueSensors = new Set();
    for (const event of events) {
      uniqueSensors.add(event.args.sensor);
    }
    participants = Array.from(uniqueSensors);
    
    console.log(`Round ${roundId}: Found ${participants.length} participants from events`);
    
    for (const addr of participants) {
      try {
        const reading = await consensusRead.getReading(roundId, addr);
        values.push(reading.value.toString());
        faultyFlags.push(reading.isFaulty);
      } catch (err) {
        console.error(`Failed to get reading for ${addr}:`, err.message);
        values.push("0");
        faultyFlags.push(false);
      }
    }
    
    try {
      const roundData = await consensusRead.getConsensusRound(roundId);
      if (roundData && roundData.timestamp && roundData.timestamp.toString() !== "0") {
        consensusReached = roundData.consensusReached;
        consensusValue = roundData.consensusValue?.toString() || "0";
        trustedParticipants = roundData.trustedParticipants?.toString() || "0";
        faultyCount = roundData.faultyCount?.toString() || "0";
        totalParticipants = roundData.totalParticipants?.toString() || "0";
        timestamp = roundData.timestamp?.toString() || "0";
        
        // If round data has values, use them instead
        if (roundData.values && roundData.values.length > 0) {
          values = Array.isArray(roundData.values) ? roundData.values.map(v => v.toString()) : values;
        }
        if (roundData.disagreementScores && roundData.disagreementScores.length > 0) {
          disagreementScores = Array.isArray(roundData.disagreementScores) ? roundData.disagreementScores.map(s => s.toString()) : [];
        }
        if (roundData.faultyFlags && roundData.faultyFlags.length > 0) {
          faultyFlags = Array.isArray(roundData.faultyFlags) ? roundData.faultyFlags : faultyFlags;
        }
      }
    } catch (err) {
      console.log(`Round ${roundId} not finalized in consensusRounds`);
    }
    
    if (disagreementScores.length === 0 && values.length >= 2) {
      const count = values.length;
      disagreementScores = new Array(count).fill(0);
      for (let i = 0; i < count - 1; i++) {
        for (let j = i + 1; j < count; j++) {
          const diff = Math.abs(Number(values[i]) - Number(values[j]));
          disagreementScores[i] += diff;
          disagreementScores[j] += diff;
        }
      }
      disagreementScores = disagreementScores.map(s => s.toString());
    }
    
  } catch (err) {
    console.error(`Failed to fetch events for round ${roundId}:`, err.message);
  }
  
  // If still no participants, try to get from consensusRounds directly
  if (participants.length === 0) {
    try {
      const roundData = await consensusRead.getConsensusRound(roundId);
      if (roundData && roundData.participants) {
        participants = Array.isArray(roundData.participants) ? roundData.participants : [];
        values = Array.isArray(roundData.values) ? roundData.values.map(v => v.toString()) : [];
        disagreementScores = Array.isArray(roundData.disagreementScores) ? roundData.disagreementScores.map(s => s.toString()) : [];
        faultyFlags = Array.isArray(roundData.faultyFlags) ? roundData.faultyFlags : [];
        consensusReached = roundData.consensusReached || false;
        consensusValue = roundData.consensusValue?.toString() || "0";
        trustedParticipants = roundData.trustedParticipants?.toString() || "0";
        faultyCount = roundData.faultyCount?.toString() || "0";
        totalParticipants = roundData.totalParticipants?.toString() || "0";
        timestamp = roundData.timestamp?.toString() || "0";
      }
    } catch (err) {
      console.log(`Round ${roundId} not found in consensusRounds`);
    }
  }
  
  const count = participants.length;
  const faultyThreshRaw = Number(await consensusRead.faultyThresholdUnits());
  const scoreThreshold = faultyThreshRaw * (Math.max(count, 1) - 1);
  
  const valuesStr = values.map(v => v.toString ? v.toString() : String(v));
  const valuesScaled = valuesStr.map(v => (Number(v) / 100).toFixed(2));
  const scoresStr = disagreementScores.map(s => s.toString ? s.toString() : String(s));
  
  console.log('Explained round data:', {
    roundId,
    participantCount: count,
    valuesCount: valuesStr.length,
    faultyCount: faultyFlags.filter(f => f === true).length
  });

  const pairwiseComparisons = [];
  for (let i = 0; i < count - 1; i++) {
    for (let j = i + 1; j < count; j++) {
      const vi = Number(valuesStr[i] || 0);
      const vj = Number(valuesStr[j] || 0);
      const diff = Math.abs(vi - vj);
      pairwiseComparisons.push({
        sensorA: participants[i],
        sensorAValue: (vi / 100).toFixed(2),
        sensorB: participants[j],
        sensorBValue: (vj / 100).toFixed(2),
        differenceRaw: diff.toString(),
        differenceScaled: (diff / 100).toFixed(2)
      });
    }
  }

  const sensorAnalysis = participants.map((addr, i) => {
    const scoreRaw = Number(scoresStr[i] || 0);
    const isFaulty = faultyFlags[i] || false;
    const excess = isFaulty ? scoreRaw - scoreThreshold : 0;
    return {
      address: addr,
      submittedValue: valuesScaled[i],
      submittedValueRaw: valuesStr[i] || "0",
      disagreementScore: scoresStr[i] || "0",
      disagreementScoreScaled: ((scoreRaw) / 100).toFixed(2),
      scoreThreshold: scoreThreshold.toString(),
      scoreThresholdScaled: (scoreThreshold / 100).toFixed(2),
      isFaulty,
      verdict: isFaulty
        ? `FAULTY  score ${(scoreRaw/100).toFixed(2)} exceeds threshold ${(scoreThreshold/100).toFixed(2)} by ${(excess/100).toFixed(2)}`
        : `TRUSTED  score ${(scoreRaw/100).toFixed(2)} within threshold ${(scoreThreshold/100).toFixed(2)}`
    };
  });

  const trustedSensors = sensorAnalysis.filter(s => !s.isFaulty);
  const trustedSum = trustedSensors.reduce((acc, s) => acc + Number(s.submittedValueRaw), 0);
  const trustedAvgRaw = trustedSensors.length > 0 ? Math.trunc(trustedSum / trustedSensors.length) : 0;
  
  const finalFaultyCount = faultyFlags.filter(f => f === true).length;
  const finalConsensusReached = consensusReached || (count >= 2 && finalFaultyCount <= Math.floor(count / 2));

  let txTrace = null;
  try {
    const eventFilter = finalConsensusReached
      ? consensusRead.filters.ConsensusReached(roundId)
      : consensusRead.filters.ConsensusRejected(roundId);
    const events = await consensusRead.queryFilter(eventFilter, 0, 'latest');
    if (events.length > 0) {
      const ev = events[0];
      const block = await provider.getBlock(ev.blockNumber);
      txTrace = {
        transactionHash: ev.transactionHash,
        blockNumber: ev.blockNumber,
        blockHash: ev.blockHash,
        blockTimestamp: block.timestamp.toString(),
        blockTimestampISO: new Date(block.timestamp * 1000).toISOString(),
        eventName: finalConsensusReached ? "ConsensusReached" : "ConsensusRejected"
      };
    }
  } catch { /* best-effort */ }

  return {
    roundId: roundId.toString(),

    inputs: {
      sensorCount: count,
      sensors: participants.map((addr, i) => ({
        address: addr,
        valueRaw: valuesStr[i] || "0",
        valueScaled: valuesScaled[i]
      }))
    },

    step1_pairwiseComparisons: {
      description: "For every pair (i,j): diff = |value_i - value_j|. Both sensors accumulate diff in their disagreement score.",
      totalPairs: pairwiseComparisons.length,
      pairs: pairwiseComparisons
    },

    step2_faultDetection: {
      description: "A sensor is FAULTY if its total disagreement score exceeds faultyThresholdUnits  (count1).",
      faultyThresholdUnits: faultyThreshRaw.toString(),
      faultyThresholdScaled: (faultyThreshRaw / 100).toFixed(2),
      scoreThresholdRaw: scoreThreshold.toString(),
      scoreThresholdScaled: (scoreThreshold / 100).toFixed(2),
      formula: `threshold = ${faultyThreshRaw}  (${count}1) = ${scoreThreshold}`,
      sensors: sensorAnalysis
    },

    step3_safetyCheck: {
      description: "Round is REJECTED if faultyCount > totalCount / 2 (majority faulty).",
      totalSensors: count,
      faultySensors: finalFaultyCount,
      majorityLimit: Math.floor(count / 2),
      roundRejected: !finalConsensusReached && count > 0,
      verdict: finalFaultyCount > Math.floor(count / 2)
        ? `REJECTED  ${finalFaultyCount} faulty sensors exceeds majority limit of ${Math.floor(count/2)}`
        : `PASSED  ${finalFaultyCount} faulty sensors within majority limit of ${Math.floor(count/2)}`
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
      storedConsensusRaw: consensusValue,
      storedConsensusScaled: (Number(consensusValue) / 100).toFixed(2)
    },

    result: {
      consensusReached: finalConsensusReached,
      consensusValue: consensusValue,
      consensusValueScaled: (Number(consensusValue) / 100).toFixed(2),
      trustedParticipants: trustedSensors.length.toString(),
      faultyCount: finalFaultyCount.toString(),
      totalParticipants: count.toString()
    },

    blockchainTrace: txTrace
  };
}


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


module.exports = {
  registerDevice,
  updateFirmware,
  deactivateDevice,
  reactivateDevice,
  getAllDevices,
  getDeviceInfo,
  verifyDevice,
  verifyFirmware,
  getDeviceFirmware,
  isDeviceRegistered,
  getRegistryStats,
  submitReading,
  forceNewRound,
  forceConsensusCalculation,
  setFaultyThreshold,
  setMinSensorsForConsensus,
  setConsensusWindow,
  setDeviceRegistry,
  getLatestConsensus,
  getLatestRound,
  getConsensusRound,
  getCurrentRoundParticipants,
  getCurrentRoundDetails,
  getReading,
  getConsensusStats,
  getAllRounds,
  explainConsensusRound,
  getLatestBlock,
  getBlockByNumber,
  getTransactionDetails,
  getNetworkInfo,
  getEventHistory,
  formatBlockchainError
};