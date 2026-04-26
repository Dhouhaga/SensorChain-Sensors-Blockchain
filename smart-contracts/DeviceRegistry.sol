// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

/**
 * @title DeviceRegistry
 * @dev Manages IoT device registration, firmware integrity verification, and device authentication.
 *
 * Security Features:
 * - Each device registered with a unique Ethereum address (public key)
 * - Firmware hash stored on-chain for integrity verification
 * - Version control for firmware updates
 * - Only the contract owner can register devices or update firmware
 * - Prevents unauthorized devices from participating in the sensor network
 */
contract DeviceRegistry {

    // ==================== STRUCTS ====================

    /**
     * @dev Represents an IoT device in the system.
     */
    struct Device {
        address deviceAddress;    // Unique Ethereum address of the device
        bytes32 firmwareHash;     // SHA-256 hash of the current authorized firmware
        uint256 firmwareVersion;  // Firmware version number (must be incremental)
        string deviceType;        // Human-readable type, e.g. "Temperature Sensor"
        bool isActive;            // Whether the device is currently allowed to submit data
        uint256 registrationTime; // Block timestamp at registration
        uint256 lastUpdate;       // Block timestamp of the last firmware update
    }

    // ==================== STATE VARIABLES ====================

    /// @notice Mapping from device address to its Device record
    mapping(address => Device) public devices;

    /// @notice Ordered list of all registered device addresses
    address[] public deviceList;

    /// @notice The admin / manufacturer who deployed this contract
    address public contractOwner;

    /// @notice Running total of registered devices
    uint256 public totalDevices;

    // ==================== EVENTS ====================

    event DeviceRegistered(
        address indexed deviceAddress,
        bytes32 firmwareHash,
        uint256 firmwareVersion,
        string deviceType,
        uint256 timestamp
    );

    event FirmwareUpdated(
        address indexed deviceAddress,
        bytes32 oldFirmwareHash,
        bytes32 newFirmwareHash,
        uint256 newVersion,
        uint256 timestamp
    );

    event DeviceDeactivated(address indexed deviceAddress, uint256 timestamp);
    event DeviceReactivated(address indexed deviceAddress, uint256 timestamp);

    // ==================== MODIFIERS ====================

    modifier onlyOwner() {
        require(msg.sender == contractOwner, "Only contract owner can call this function");
        _;
    }

    // ==================== CONSTRUCTOR ====================

    constructor() {
        contractOwner = msg.sender;
    }

    // ==================== CORE FUNCTIONS ====================

    /**
     * @notice Register a new IoT device (admin only).
     * @param _deviceAddress  The Ethereum address that represents the physical device.
     * @param _firmwareHash   SHA-256 hash of the device's authorized firmware image.
     * @param _firmwareVersion Initial firmware version number (use 1 for a new device).
     * @param _deviceType     Human-readable device category, e.g. "Temperature Sensor".
     */
    function registerDevice(
        address _deviceAddress,
        bytes32 _firmwareHash,
        uint256 _firmwareVersion,
        string memory _deviceType
    ) external onlyOwner {
        require(_deviceAddress != address(0), "Invalid device address");
        require(devices[_deviceAddress].deviceAddress == address(0), "Device already registered");
        require(_firmwareHash != bytes32(0), "Firmware hash cannot be empty");
        require(_firmwareVersion > 0, "Firmware version must be greater than 0");

        devices[_deviceAddress] = Device({
            deviceAddress: _deviceAddress,
            firmwareHash: _firmwareHash,
            firmwareVersion: _firmwareVersion,
            deviceType: _deviceType,
            isActive: true,
            registrationTime: block.timestamp,
            lastUpdate: block.timestamp
        });

        deviceList.push(_deviceAddress);
        totalDevices++;

        emit DeviceRegistered(_deviceAddress, _firmwareHash, _firmwareVersion, _deviceType, block.timestamp);
    }

    /**
     * @notice Update the firmware record for a registered device (admin only).
     * @dev The new version must be strictly greater than the current version.
     * @param _deviceAddress  Address of the device whose firmware is being updated.
     * @param _newFirmwareHash SHA-256 hash of the new firmware image.
     * @param _newVersion      New firmware version number.
     */
    function updateFirmware(
        address _deviceAddress,
        bytes32 _newFirmwareHash,
        uint256 _newVersion
    ) external onlyOwner {
        require(devices[_deviceAddress].deviceAddress != address(0), "Device not registered");
        require(_newFirmwareHash != bytes32(0), "Firmware hash cannot be empty");

        Device storage device = devices[_deviceAddress];

        require(_newVersion > device.firmwareVersion, "New version must be greater than current version");
        require(_newFirmwareHash != device.firmwareHash, "New firmware hash must be different");

        bytes32 oldHash = device.firmwareHash;

        device.firmwareHash = _newFirmwareHash;
        device.firmwareVersion = _newVersion;
        device.lastUpdate = block.timestamp;

        emit FirmwareUpdated(_deviceAddress, oldHash, _newFirmwareHash, _newVersion, block.timestamp);
    }

    /**
     * @notice Deactivate a registered device so it can no longer submit readings (admin only).
     */
    function deactivateDevice(address _deviceAddress) external onlyOwner {
        require(devices[_deviceAddress].deviceAddress != address(0), "Device not registered");
        require(devices[_deviceAddress].isActive, "Device already inactive");

        devices[_deviceAddress].isActive = false;

        emit DeviceDeactivated(_deviceAddress, block.timestamp);
    }

    /**
     * @notice Reactivate a previously deactivated device (admin only).
     */
    function reactivateDevice(address _deviceAddress) external onlyOwner {
        require(devices[_deviceAddress].deviceAddress != address(0), "Device not registered");
        require(!devices[_deviceAddress].isActive, "Device already active");

        devices[_deviceAddress].isActive = true;

        emit DeviceReactivated(_deviceAddress, block.timestamp);
    }

    // ==================== VIEW FUNCTIONS ====================

    /**
     * @notice Returns true if the device is registered AND currently active.
     * @dev This is the primary function called by SensorConsensus for access control.
     */
    function verifyDevice(address _deviceAddress) external view returns (bool) {
        Device memory d = devices[_deviceAddress];
        return (d.deviceAddress != address(0) && d.isActive);
    }

    /**
     * @notice Returns true if the supplied firmware hash matches the device's on-chain record.
     */
    function verifyFirmware(address _deviceAddress, bytes32 _firmwareHash) external view returns (bool) {
        return devices[_deviceAddress].firmwareHash == _firmwareHash;
    }

    /**
     * @notice Returns the full Device struct for a registered device.
     * @dev SensorConsensus calls this to retrieve the authorised firmware hash.
     *      The return type is a `Device` struct  the IDeviceRegistry interface must match this exactly.
     */
    function getDeviceInfo(address _deviceAddress) external view returns (Device memory) {
        require(devices[_deviceAddress].deviceAddress != address(0), "Device not found");
        return devices[_deviceAddress];
    }

    /**
     * @notice Returns firmware details for a device.
     */
    function getDeviceFirmware(address _deviceAddress) external view returns (
        bytes32 firmwareHash,
        uint256 firmwareVersion,
        uint256 lastUpdate
    ) {
        require(devices[_deviceAddress].deviceAddress != address(0), "Device not found");
        Device memory d = devices[_deviceAddress];
        return (d.firmwareHash, d.firmwareVersion, d.lastUpdate);
    }

    /**
     * @notice Returns true if the address has ever been registered (active or not).
     */
    function isDeviceRegistered(address _deviceAddress) external view returns (bool) {
        return devices[_deviceAddress].deviceAddress != address(0);
    }

    /**
     * @notice Returns the full list of registered device addresses.
     */
    function getAllDevices() external view returns (address[] memory) {
        return deviceList;
    }
}
