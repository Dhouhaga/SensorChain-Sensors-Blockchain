// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

/**
 * @title SensorConsensus
 * @dev Faulty-sensor detection via pairwise disagreement scoring, then trusted average consensus.
 *
 * Algorithm:
 *   Step 1  Pairwise scoring.
 *            For every pair of sensors (i, j), compute |value_i - value_j|.
 *            Add that difference to BOTH sensors' disagreement scores.
 *            A sensor that consistently disagrees with everyone accumulates a high score.
 *
 *   Step 2  Fault flagging.
 *            A sensor is FAULTY if its disagreement score exceeds:
 *                faultyThresholdUnits * (participantCount - 1)
 *            Multiplying by (n-1) normalises for group size: the threshold stays
 *            "per comparison" regardless of how many sensors are in the round.
 *
 *   Step 3  Safety check.
 *            If more than half the sensors are faulty, the round is REJECTED.
 *            The network cannot produce a trustworthy value when the majority disagrees.
 *
 *   Step 4  Trusted average.
 *            Compute the average of non-faulty sensors only.
 *            This is the final consensus value stored on-chain.
 *
 * Why pairwise scoring beats single-pass average:
 *   With average-based detection, a faulty sensor pulls the reference average toward
 *   itself, shrinking its own measured deviation and potentially escaping detection.
 *   Pairwise scoring has no such blind spot: a faulty sensor is penalised directly
 *   for every sensor it disagrees with, regardless of what the average looks like.
 *
 * Value scale:
 *   All readings are submitted scaled x100 to support two decimal places in pure
 *   integer arithmetic (e.g. 25.50 C -> submit 2550).
 *   faultyThresholdUnits is also scaled x100 (default 500 = 5.00 degrees).
 */

// Interface  must mirror DeviceRegistry EXACTLY
interface IDeviceRegistry {
    struct Device {
        address deviceAddress;
        bytes32 firmwareHash;
        uint256 firmwareVersion;
        string  deviceType;
        bool    isActive;
        uint256 registrationTime;
        uint256 lastUpdate;
    }

    function verifyDevice(address _deviceAddress) external view returns (bool);
    function getDeviceInfo(address _deviceAddress) external view returns (Device memory);
}

contract SensorConsensus {

    // ==================== STRUCTS ====================

    struct Reading {
        address sensor;     // Address of the submitting sensor
        int256  value;      // Value scaled x100 (e.g. 2550 = 25.50 C)
        uint256 timestamp;  // Signed timestamp supplied by the device
        bool    isFaulty;   // True if this reading was excluded from consensus
    }

    struct ConsensusRound {
        uint256   roundId;
        uint256   timestamp;             // When consensus was calculated
        int256    consensusValue;        // Trusted average (non-faulty sensors only)
        uint256   totalParticipants;     // Total sensors that submitted
        uint256   trustedParticipants;   // Sensors NOT flagged as faulty
        uint256   faultyCount;           // Sensors flagged as faulty
        bool      consensusReached;      // False if round was rejected
        address[] participants;          // All sensor addresses (insertion order)
        int256[]  values;                // Their values (same order)
        int256[]  disagreementScores;    // Pairwise score per sensor (same order)
        bool[]    faultyFlags;           // Faulty flag per sensor (same order)
    }

    // ==================== STATE VARIABLES ====================

    IDeviceRegistry public deviceRegistry;

    /// @notice Minimum sensors required before consensus can run.
    uint256 public minSensorsForConsensus = 3;

    /// @notice Per-comparison fault threshold, scaled x100.
    ///         faultyThresholdUnits * (participantCount - 1).
    ///         Default 500 = 5.00 degrees per comparison.
    int256  public faultyThresholdUnits = 500;

    uint256 public currentRoundId;
    uint256 public consensusWindow = 3600; // seconds
    uint256 public currentRoundStartTime;
    uint256 public totalRounds;
    address public owner;

    mapping(uint256 => ConsensusRound)              public consensusRounds;
    mapping(uint256 => mapping(address => Reading)) public roundReadings;
    mapping(uint256 => address[])                   public roundParticipants;

    // ==================== EVENTS ====================

    event ReadingSubmitted(
        uint256 indexed roundId,
        address indexed sensor,
        int256  value,
        uint256 timestamp
    );

    /// @dev Fired for each sensor eliminated by pairwise scoring.
    event FaultySensorDetected(
        uint256 indexed roundId,
        address indexed sensor,
        int256  submittedValue,
        int256  disagreementScore,
        int256  scoreThreshold,
        uint256 timestamp
    );

    /// @dev Fired when a trusted average is successfully produced.
    event ConsensusReached(
        uint256 indexed roundId,
        int256  consensusValue,
        uint256 trustedParticipants,
        uint256 faultyCount,
        uint256 timestamp
    );

    /// @dev Fired when the round is rejected because the majority are faulty.
    event ConsensusRejected(
        uint256 indexed roundId,
        uint256 totalParticipants,
        uint256 faultyCount,
        string  reason,
        uint256 timestamp
    );

    event NewRoundStarted(uint256 indexed roundId, uint256 timestamp);

    // ==================== MODIFIERS ====================

    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner can call this function");
        _;
    }

    // ==================== CONSTRUCTOR ====================

    constructor(address _deviceRegistryAddress) {
        require(_deviceRegistryAddress != address(0), "Invalid registry address");
        deviceRegistry        = IDeviceRegistry(_deviceRegistryAddress);
        owner                 = msg.sender;
        currentRoundId        = 1;
        currentRoundStartTime = block.timestamp;
        emit NewRoundStarted(currentRoundId, block.timestamp);
    }

    // ==================== CORE FUNCTIONS ====================

    /**
     * @notice Submit a sensor reading for the current consensus round.
     *
     * Security pipeline:
     *   1. Device must be registered and active in DeviceRegistry.
     *   2. Supplied firmware hash must match the authorised hash in DeviceRegistry.
     *   3. Auto-advance the round if the time window has expired.
     *   4. Duplicate submission guard.
     *   5. Cryptographic signature verification.
     *   6. Store the reading.
     *   7. Auto-trigger consensus once minSensorsForConsensus readings are in.
     *
     * @param _value        Reading scaled x100 (e.g. 2550 for 25.50 C).
     * @param _timestamp    Timestamp the device embedded in its signed message.
     * @param _firmwareHash Firmware hash currently running on the device.
     * @param _signature    65-byte ECDSA signature of keccak256(abi.encodePacked(_value, _timestamp)).
     */
    function submitReading(
        int256       _value,
        uint256      _timestamp,
        bytes32      _firmwareHash,
        bytes memory _signature
    ) external {

        // 1. Registry check
        require(deviceRegistry.verifyDevice(msg.sender), "Device not registered or inactive");

        // 2. Firmware integrity check
        IDeviceRegistry.Device memory dev = deviceRegistry.getDeviceInfo(msg.sender);
        require(_firmwareHash == dev.firmwareHash, "Security Alert: Firmware hash mismatch");

        // 3. Round management
        if (block.timestamp - currentRoundStartTime > consensusWindow) {
            _finalizeCurrentRound();
            _startNewRound();
        }

        // 4. Duplicate submission guard
        require(
            roundReadings[currentRoundId][msg.sender].sensor == address(0),
            "Device already submitted for this round"
        );

        // 5. Signature verification
        bytes32 messageHash   = keccak256(abi.encodePacked(_value, _timestamp));
        bytes32 ethSignedHash = _getEthSignedMessageHash(messageHash);
        address recovered     = _recoverSigner(ethSignedHash, _signature);
        require(recovered == msg.sender, "Security Alert: Invalid cryptographic signature");

        // 6. Store reading
        roundReadings[currentRoundId][msg.sender] = Reading({
            sensor:    msg.sender,
            value:     _value,
            timestamp: _timestamp,
            isFaulty:  false
        });
        roundParticipants[currentRoundId].push(msg.sender);
        emit ReadingSubmitted(currentRoundId, msg.sender, _value, _timestamp);

        // 7. Auto-trigger consensus
        if (
            roundParticipants[currentRoundId].length >= minSensorsForConsensus &&
            !consensusRounds[currentRoundId].consensusReached
        ) {
            _calculateConsensus();
        }
    }

    // ==================== INTERNAL CONSENSUS LOGIC ====================

    /**
     * @dev Pairwise disagreement scoring consensus.
     *
     *  Step 1  Build disagreement scores.
     *           For every unique pair (i, j): diff = |value_i - value_j|
     *           scores[i] += diff
     *           scores[j] += diff
     *
     *  Step 2  Flag faulty sensors.
     *           Threshold = faultyThresholdUnits * (count - 1)
     *           If scores[i] > threshold -> sensor i is FAULTY.
     *
     *  Step 3  Safety check.
     *           If faultyCount > count / 2 -> reject the round entirely.
     *
     *  Step 4  Trusted average.
     *           Average of non-faulty values only -> consensusValue.
     */
    function _calculateConsensus() internal {
        address[] memory participants = roundParticipants[currentRoundId];
        uint256 count = participants.length;

        int256[] memory values = new int256[](count);
        for (uint256 i = 0; i < count; i++) {
            values[i] = roundReadings[currentRoundId][participants[i]].value;
        }

        //  Step 1: pairwise disagreement scores 
        int256[] memory scores = new int256[](count);

        for (uint256 i = 0; i < count - 1; i++) {
            for (uint256 j = i + 1; j < count; j++) {
                int256 diff = values[i] > values[j]
                    ? values[i] - values[j]
                    : values[j] - values[i];
                scores[i] += diff;
                scores[j] += diff;
            }
        }

        //  Step 2: fault flagging 
        // multiplied by (count-1), i.e. faultyThresholdUnits per comparison.
        int256  scoreThreshold = faultyThresholdUnits * int256(count - 1);
        bool[]  memory faultyFlags = new bool[](count);
        uint256 faultyCount = 0;

        for (uint256 i = 0; i < count; i++) {
            if (scores[i] > scoreThreshold) {
                faultyFlags[i] = true;
                faultyCount++;
                roundReadings[currentRoundId][participants[i]].isFaulty = true;

                emit FaultySensorDetected(
                    currentRoundId,
                    participants[i],
                    values[i],
                    scores[i],
                    scoreThreshold,
                    block.timestamp
                );
            }
        }

        //  Step 3: safety check 
        if (faultyCount > count / 2) {
            consensusRounds[currentRoundId] = ConsensusRound({
                roundId:            currentRoundId,
                timestamp:          block.timestamp,
                consensusValue:     0,
                totalParticipants:  count,
                trustedParticipants: count - faultyCount,
                faultyCount:        faultyCount,
                consensusReached:   false,
                participants:       participants,
                values:             values,
                disagreementScores: scores,
                faultyFlags:        faultyFlags
            });

            emit ConsensusRejected(
                currentRoundId,
                count,
                faultyCount,
                "Majority of sensors are faulty - round rejected",
                block.timestamp
            );
            return;
        }

        //  Step 4: trusted average 
        int256  trustedSum   = 0;
        uint256 trustedCount = 0;
        for (uint256 i = 0; i < count; i++) {
            if (!faultyFlags[i]) {
                trustedSum += values[i];
                trustedCount++;
            }
        }
        int256 consensusValue = trustedSum / int256(trustedCount);

        //  Persist 
        consensusRounds[currentRoundId] = ConsensusRound({
            roundId:            currentRoundId,
            timestamp:          block.timestamp,
            consensusValue:     consensusValue,
            totalParticipants:  count,
            trustedParticipants: trustedCount,
            faultyCount:        faultyCount,
            consensusReached:   true,
            participants:       participants,
            values:             values,
            disagreementScores: scores,
            faultyFlags:        faultyFlags
        });

        emit ConsensusReached(
            currentRoundId,
            consensusValue,
            trustedCount,
            faultyCount,
            block.timestamp
        );
    }

    function _finalizeCurrentRound() internal {
        if (
            roundParticipants[currentRoundId].length >= minSensorsForConsensus &&
            !consensusRounds[currentRoundId].consensusReached
        ) {
            _calculateConsensus();
        }
        totalRounds++;
    }

    function _startNewRound() internal {
        currentRoundId++;
        currentRoundStartTime = block.timestamp;
        emit NewRoundStarted(currentRoundId, block.timestamp);
    }

    // ==================== INTERNAL SIGNATURE HELPERS ====================

    function _getEthSignedMessageHash(bytes32 _hash) internal pure returns (bytes32) {
        return keccak256(abi.encodePacked("\x19Ethereum Signed Message:\n32", _hash));
    }

    function _recoverSigner(bytes32 _ethSignedHash, bytes memory _sig) internal pure returns (address) {
        (bytes32 r, bytes32 s, uint8 v) = _splitSignature(_sig);
        return ecrecover(_ethSignedHash, v, r, s);
    }

    function _splitSignature(bytes memory _sig) internal pure returns (bytes32 r, bytes32 s, uint8 v) {
        require(_sig.length == 65, "Invalid signature length");
        assembly {
            r := mload(add(_sig, 32))
            s := mload(add(_sig, 64))
            v := byte(0, mload(add(_sig, 96)))
        }
    }

    // ==================== ADMIN FUNCTIONS ====================

    /**
     * @notice Manually force consensus for the current round (owner only).
     */
    function forceConsensusCalculation() external onlyOwner {
        require(
            roundParticipants[currentRoundId].length >= minSensorsForConsensus,
            "Not enough participants"
        );
        require(
            !consensusRounds[currentRoundId].consensusReached,
            "Consensus already reached for this round"
        );
        _calculateConsensus();
    }

    /**
     * @notice Manually close the current round and open a new one (owner only).
     */
    function forceNewRound() external onlyOwner {
        _finalizeCurrentRound();
        _startNewRound();
    }

    /**
     * @notice Set the per-comparison fault threshold, scaled x100.
     *         Examples: 500 = 5.00 degrees, 250 = 2.50 degrees, 100 = 1.00 degree.
     *         A sensor is flagged when its total disagreement score exceeds
     *         this value multiplied by (participantCount - 1).
     */
    function setFaultyThreshold(int256 _threshold) external onlyOwner {
        require(_threshold > 0, "Threshold must be positive");
        faultyThresholdUnits = _threshold;
    }

    /// @notice Update the minimum sensor count required for consensus (min 2).
    function setMinSensorsForConsensus(uint256 _minSensors) external onlyOwner {
        require(_minSensors >= 2, "Minimum must be at least 2");
        minSensorsForConsensus = _minSensors;
    }

    /// @notice Update the consensus collection window (minimum 60 seconds).
    function setConsensusWindow(uint256 _windowSeconds) external onlyOwner {
        require(_windowSeconds >= 60, "Window must be at least 60 seconds");
        consensusWindow = _windowSeconds;
    }

    /// @notice Point this contract at a new DeviceRegistry deployment.
    function setDeviceRegistry(address _newRegistry) external onlyOwner {
        require(_newRegistry != address(0), "Invalid registry address");
        deviceRegistry = IDeviceRegistry(_newRegistry);
    }

    // ==================== VIEW FUNCTIONS ====================

    /**
     * @notice Returns the most recently finalised trusted consensus value.
     * @return consensusValue    Trusted average (non-faulty sensors only), scaled x100.
     * @return trustedCount      Number of sensors whose readings were used.
     * @return faultyCount       Number of sensors that were excluded.
     * @return timestamp         When consensus was calculated.
     */
    function getLatestConsensus() external view returns (
        int256  consensusValue,
        uint256 trustedCount,
        uint256 faultyCount,
        uint256 timestamp
    ) {
        uint256 rid = currentRoundId;
        while (rid > 0) {
            if (consensusRounds[rid].consensusReached) {
                ConsensusRound memory r = consensusRounds[rid];
                return (r.consensusValue, r.trustedParticipants, r.faultyCount, r.timestamp);
            }
            rid--;
        }
        return (0, 0, 0, 0);
    }

    /**
     * @notice Returns the full ConsensusRound for a given round ID.
     *         Includes disagreementScores so you can see exactly why each sensor
     *         was trusted or flagged.
     *         If consensusReached is false, the round was rejected.
     */
    function getConsensusRound(uint256 _roundId) external view returns (ConsensusRound memory) {
        require(_roundId > 0 && _roundId <= currentRoundId, "Invalid round ID");
        return consensusRounds[_roundId];
    }

    /// @notice Returns all sensor addresses that have submitted in the current round.
    function getCurrentRoundParticipants() external view returns (address[] memory) {
        return roundParticipants[currentRoundId];
    }

    /**
     * @notice Returns the Reading for a specific sensor in a specific round.
     *         Check isFaulty to see if this sensor was excluded from consensus.
     */
    function getReading(uint256 _roundId, address _sensor) external view returns (Reading memory) {
        return roundReadings[_roundId][_sensor];
    }

    /**
 * @notice Returns the most recent round result  successful or rejected.
 *         Unlike getLatestConsensus which skips rejected rounds, this always
 *         returns what actually happened last.
 */
function getLatestRound() external view returns (
    uint256 roundId,
    bool    consensusReached,
    int256  consensusValue,
    uint256 trustedParticipants,
    uint256 faultyCount,
    uint256 timestamp
) {
    uint256 rid = currentRoundId;
    while (rid > 0) {
        ConsensusRound memory r = consensusRounds[rid];
        // A round is "processed" if it has a timestamp (i.e. consensus was attempted)
        if (r.timestamp > 0) {
            return (
                r.roundId,
                r.consensusReached,
                r.consensusValue,
                r.trustedParticipants,
                r.faultyCount,
                r.timestamp
            );
        }
        rid--;
    }
    return (0, false, 0, 0, 0, 0);
}

/**
 * @notice Returns full details of all submissions in the current round.
 *         Single call replaces N getReading() calls.
 */
function getCurrentRoundDetails() external view returns (
    address[] memory participants,
    int256[]  memory values,
    bool[]    memory faultyFlags
) {
    address[] memory parts = roundParticipants[currentRoundId];
    uint256 count = parts.length;

    int256[] memory vals   = new int256[](count);
    bool[]   memory faulty = new bool[](count);

    for (uint256 i = 0; i < count; i++) {
        Reading memory r = roundReadings[currentRoundId][parts[i]];
        vals[i]   = r.value;
        faulty[i] = r.isFaulty;
    }

    return (parts, vals, faulty);
}
}
