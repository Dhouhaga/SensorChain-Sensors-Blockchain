# SensorChain

SensorChain is an IoT blockchain demo that combines a Node.js backend, a Qt desktop frontend, and two Solidity smart contracts to register devices, submit signed readings, and calculate consensus on-chain.

<img width="1366" height="676" alt="result(1)" src="https://github.com/user-attachments/assets/08c3d7a9-3253-4999-9698-11b332c7a962" />


The project is structured for local blockchain development with Ganache, but it is also suitable for publishing as a public repository because private secrets are kept out of version control and the repository includes a safe environment template.

## Architecture

- Smart contracts: `smart-contracts/DeviceRegistry.sol` and `smart-contracts/SensorConsensus.sol`
- Backend: `backend/` exposes HTTP endpoints, signs sensor submissions, and reads contract data through ethers.js
- Desktop app: `Qt-frontend/SensorChain/` provides the operator UI for devices, sensors, consensus, and blockchain inspection

## Requirements

- Node.js 18 or newer
- Ganache or another local Ethereum JSON-RPC endpoint
- Qt 6 with Widgets and Network modules
- A Solidity compiler and deployment workflow for the contracts

## Quick Start

### 1. Deploy the contracts

Deploy `DeviceRegistry.sol` first, then deploy `SensorConsensus.sol` with the registry address.

Keep the deployed addresses for the backend configuration.

### 2. Configure the backend

Create a local env file from the template:

```bash
cd backend
copy .env.example .env
```

Fill in real values in `.env` for:

- `RPC_URL`
- `DEVICE_REGISTRY_ADDRESS`
- `SENSOR_CONSENSUS_ADDRESS`
- `OWNER_PRIVATE_KEY`
- `SENSOR_PRIVATE_KEY_<SENSOR_ADDRESS>` entries for each sensor wallet

Do not commit `.env`.

### 3. Run the backend

```bash
cd backend
npm install
npm run dev
```

The server starts on port `3000` by default.

### 4. Run the Qt desktop app

Open `Qt-frontend/SensorChain/` in Qt Creator or build it with CMake:

Qt links against the backend over HTTP.

## Backend API

The backend exposes three route groups:

- `/registry` for `DeviceRegistry`
- `/consensus` for `SensorConsensus`
- `/blockchain` for chain, block, and transaction inspection

Open `http://localhost:3000/` after starting the backend to see the endpoint map.
