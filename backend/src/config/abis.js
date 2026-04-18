// ============================================================
// Contract ABIs
// Generated from DeviceRegistry.sol and SensorConsensus.sol
// ============================================================

const DEVICE_REGISTRY_ABI = [
  // ── Write functions (onlyOwner) ──────────────────────────────────────────
  "function registerDevice(address _deviceAddress, bytes32 _firmwareHash, uint256 _firmwareVersion, string memory _deviceType) external",
  "function updateFirmware(address _deviceAddress, bytes32 _newFirmwareHash, uint256 _newVersion) external",
  "function deactivateDevice(address _deviceAddress) external",
  "function reactivateDevice(address _deviceAddress) external",

  // ── Read functions ───────────────────────────────────────────────────────
  "function verifyDevice(address _deviceAddress) external view returns (bool)",
  "function verifyFirmware(address _deviceAddress, bytes32 _firmwareHash) external view returns (bool)",
  "function getDeviceInfo(address _deviceAddress) external view returns (tuple(address deviceAddress, bytes32 firmwareHash, uint256 firmwareVersion, string deviceType, bool isActive, uint256 registrationTime, uint256 lastUpdate))",
  "function getDeviceFirmware(address _deviceAddress) external view returns (bytes32 firmwareHash, uint256 firmwareVersion, uint256 lastUpdate)",
  "function isDeviceRegistered(address _deviceAddress) external view returns (bool)",
  "function getAllDevices() external view returns (address[])",

  // ── State variables (auto-generated getters) ─────────────────────────────
  "function contractOwner() external view returns (address)",
  "function totalDevices() external view returns (uint256)",

  // ── Events ───────────────────────────────────────────────────────────────
  "event DeviceRegistered(address indexed deviceAddress, bytes32 firmwareHash, uint256 firmwareVersion, string deviceType, uint256 timestamp)",
  "event FirmwareUpdated(address indexed deviceAddress, bytes32 oldFirmwareHash, bytes32 newFirmwareHash, uint256 newVersion, uint256 timestamp)",
  "event DeviceDeactivated(address indexed deviceAddress, uint256 timestamp)",
  "event DeviceReactivated(address indexed deviceAddress, uint256 timestamp)"
];

const SENSOR_CONSENSUS_ABI = [
  // ── Write functions (sensor) ─────────────────────────────────────────────
  "function submitReading(int256 _value, uint256 _timestamp, bytes32 _firmwareHash, bytes memory _signature) external",

  // ── Write functions (onlyOwner) ──────────────────────────────────────────
  "function forceConsensusCalculation() external",
  "function forceNewRound() external",
  "function setFaultyThreshold(int256 _threshold) external",
  "function setMinSensorsForConsensus(uint256 _minSensors) external",
  "function setConsensusWindow(uint256 _windowSeconds) external",
  "function setDeviceRegistry(address _newRegistry) external",

  // ── Read functions ───────────────────────────────────────────────────────
  "function getLatestConsensus() external view returns (int256 consensusValue, uint256 trustedCount, uint256 faultyCount, uint256 timestamp)",
  "function getConsensusRound(uint256 _roundId) external view returns (tuple(uint256 roundId, uint256 timestamp, int256 consensusValue, uint256 totalParticipants, uint256 trustedParticipants, uint256 faultyCount, bool consensusReached, address[] participants, int256[] values, int256[] disagreementScores, bool[] faultyFlags))",
  "function getCurrentRoundParticipants() external view returns (address[])",
  "function getCurrentRoundDetails() external view returns (address[] participants, int256[] values, bool[] faultyFlags)",
  "function getReading(uint256 _roundId, address _sensor) external view returns (tuple(address sensor, int256 value, uint256 timestamp, bool isFaulty))",
  "function getLatestRound() external view returns (uint256, bool, int256, uint256, uint256, uint256, uint256)",

  // ── State variables (auto-generated getters) ─────────────────────────────
  "function currentRoundId() external view returns (uint256)",
  "function currentRoundStartTime() external view returns (uint256)",
  "function consensusWindow() external view returns (uint256)",
  "function minSensorsForConsensus() external view returns (uint256)",
  "function faultyThresholdUnits() external view returns (int256)",
  "function totalRounds() external view returns (uint256)",
  "function owner() external view returns (address)",
  "function deviceRegistry() external view returns (address)",

  // ── Events ───────────────────────────────────────────────────────────────
  "event ReadingSubmitted(uint256 indexed roundId, address indexed sensor, int256 value, uint256 timestamp)",
  "event FaultySensorDetected(uint256 indexed roundId, address indexed sensor, int256 submittedValue, int256 disagreementScore, int256 scoreThreshold, uint256 timestamp)",
  "event ConsensusReached(uint256 indexed roundId, int256 consensusValue, uint256 trustedParticipants, uint256 faultyCount, uint256 timestamp)",
  "event ConsensusRejected(uint256 indexed roundId, uint256 totalParticipants, uint256 faultyCount, string reason, uint256 timestamp)",
  "event NewRoundStarted(uint256 indexed roundId, uint256 timestamp)"
];

module.exports = { DEVICE_REGISTRY_ABI, SENSOR_CONSENSUS_ABI };
