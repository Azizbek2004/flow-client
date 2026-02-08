# FLOW Architecture Analysis

## Overview
FLOW is based on Amnezia VPN client. This document outlines the key components of the existing architecture and how FLOW integrates with them.

## Key Files & Directories

### 1. Entry Point
- **File:** `client/main.cpp`
- **Purpose:** Initializes the Qt application, loads the QML engine, and starts the main event loop.

### 2. User Interface
- **Directory:** `client/ui/qml`
- **Main File:** `client/ui/qml/main2.qml`
- **Technology:** Qt QML (Quick)
- **Structure:**
  - `Pages2/`: Contains individual screens (e.g., Home, Settings).
  - `Controls2/`: Custom UI controls.
  - `Config/`: Theme and style configurations.

### 3. Core Logic
- **Connection Logic:** `client/vpnconnection.cpp` handles the state machine for connecting/disconnecting.
- **Settings:** `client/settings.cpp` manages local configuration and preferences.
- **Utilities:** `client/utilities.cpp` provides helper functions.

### 4. Protocols
- **Directory:** `client/protocols`
- **Base Class:** `VpnProtocol` (defined in `vpnprotocol.h`)
- **Implementations:**
  - `OpenVpnProtocol`
  - `WireGuardProtocol`
  - `XRayProtocol` (Existing integration to be leveraged/enhanced)
  - `AwgProtocol` (AmneziaWG)

### 5. Backend/Service
- **Directory:** `service/`
- **Purpose:** Likely handles privileged operations or background tasks (to be investigated further if needed).

## FLOW Integration Plan

### Directory Structure
FLOW specific code will reside in `src/flow/` to maintain separation from upstream Amnezia code.

- `src/flow/core/`: Core logic for FLOW (ProtocolGuard, ConnectionManager).
- `src/flow/protocols/`: Enhanced protocol handlers (specialized XRay config).
- `src/flow/measurement/`: ISP detection and latency measurement.
- `src/flow/ui/`: FLOW-specific QML components.

### modifications
- **UI:** Rebrand `main2.qml` and associated pages.
- **Protocol Guard:** Inject checks into `VpnConnection::connect()` and `VpnConnection::disconnect()`.
- **Measurement:** Integrate `ISPDetector` into the connection flow before selecting a server.
