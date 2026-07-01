# Secure Dual-Tier Electronic Voting Machine (EVM)

## 📌 Project Overview
This project implements a secure, hardware-in-the-loop Electronic Voting Machine system. It utilizes a two-tier architecture: an **ESP32-based embedded voting terminal** and a **Python-based administrative host dashboard**[cite: 1, 2]. The system ensures vote integrity through cryptographic hashing, non-volatile memory redundancy, and secure asynchronous serial authorization[cite: 1, 2].

## 📁 Repository Architecture & Source Code
Click any source file below to directly inspect the implementation:
* 📁 **`Firmware/`** (Embedded Hardware Layer)
  * [`evm.ino`](Firmware/evm.ino) — Core ESP32 firmware managing the hardware state machine, NVS storage, and SPI control[cite: 1].
* 📁 **`Software_Host/`** (Administrative Control Layer)
  * [`evm_controller.py`](Software_Host/evm_controller.py) — Python/Tkinter dashboard for voter verification and hardware logging[cite: 2].
  * [`setup_db.py`](Software_Host/setup_db.py) — Automation script initializing the local encrypted SQLite election ledger[cite: 3].

## 🛠️ Core Technologies & Tech Stack
* **Microcontroller:** ESP32 (C/C++ via Arduino Core)[cite: 1]
* **Host Software:** Python 3 (Tkinter, PySerial)[cite: 2]
* **Database:** SQLite3 (RDBMS)[cite: 2, 3]
* **Hardware Peripherals:** OLED (SH1106G via Software SPI), SD Card Module (Hardware SPI), Mechanical Switches[cite: 1].
* **Security & Data Integrity:** `mbedtls` SHA-256 Cryptographic Hash Chaining, Internal Non-Volatile Storage (NVS)[cite: 1].

## 🚀 Key Engineering Features

### 1. Finite State Machine (FSM) Architecture
The embedded firmware is driven by a robust Finite State Machine (`EVMState: LOCKED, AUTHORIZED, CANDIDATE_SELECTED, STORE_VOTE, COMPLETE, SYS_ERROR, SHOW_RESULTS`) ensuring the hardware can only accept inputs during authorized windows, preventing double-voting and race conditions[cite: 1].

### 2. Cryptographic Audit Trail
To prevent tampering, every recorded vote generates a payload containing the current tally, a timestamp, and the previous vote's hash[cite: 1]. This is processed using the `mbedtls` library to generate a continuous **SHA-256 hash chain** logged directly to the SD card filesystem[cite: 1].

### 3. Asynchronous UART Authorization Protocol
The physical voting terminal remains completely locked until it receives a specific, formatted packet (`AUTH|VoterID|CRC`) over the Serial interface from the Python host[cite: 1, 2]. The host application queries a local SQLite database to verify voter eligibility before transmitting the authorization token[cite: 2].

### 4. Memory Redundancy & Fault Tolerance
Votes are simultaneously written to the ESP32's internal Non-Volatile Storage (NVS) and external SD card storage[cite: 1]. If an SD card failure is detected during the `STORE_VOTE` state, the system safely halts execution (`SYS_ERROR`) and triggers a hardware alert[cite: 1].

### 5. Hardware Debouncing & Multi-Press Protection
Inputs are safeguarded with software-defined debounce delays and simultaneous multi-button-press rejection logic to ensure absolute voter intent accuracy[cite: 1].

## 📸 System Demonstration
Below is the real-time interaction between the physical voting appliance and the desktop dashboard application:

### Host Dashboard Initialization
<img src="Media/1" width="400" alt="Dashboard System Initialization">

### Hardware Unit Authorization Hook
<img src="Media/2" width="400" alt="ESP32 Unit Live Authorization State">

### Interactive Candidate Selection
<img src="Media/3" width="400" alt="Voter Component Interaction Input">

### Fraud Prevention & Duplicate Vote Rejection
<img src="Media/4" width="400" alt="Database Double-Voting Alert System">

### Secure Audit Tally Verification
<img src="Media/5" width="400" alt="Final Ledger Integrity Check Validation">

### 🎥 Real-Time Verification Demonstration
*(Embedded video player showcasing complete hardware-in-the-loop validation flow)*
<img src="Media/Video" width="600" alt="Live End-to-End System Demonstration Video">

## 🔌 Hardware Pinout Configuration
| ESP32 Pin | Interface | Component Connection | Notes |
| :--- | :--- | :--- | :--- |
| **GPIO 23** | Hardware SPI | SD Card Reader MOSI | Dedicated Bus Interface[cite: 1] |
| **GPIO 19** | Hardware SPI | SD Card Reader MISO | Dedicated Bus Interface[cite: 1] |
| **GPIO 18** | Hardware SPI | SD Card Reader SCK | Dedicated Bus Interface[cite: 1] |
| **GPIO 5** | Hardware SPI | SD Card Reader CS | Chip Select Line[cite: 1] |
| **GPIO 22** | Software SPI | OLED D0 / SCL | Dedicated Interface Clock[cite: 1] |
| **GPIO 21** | Software SPI | OLED D1 / SDA | Dedicated Interface Data[cite: 1] |
| **GPIO 13** | Hardware I/O | Green LED (Ready / Auth) | Visual Logic Output Status[cite: 1] |
| **GPIO 4** | Hardware I/O | Yellow LED (Processing) | Visual Logic Output Status[cite: 1] |
| **GPIO 14** | Hardware I/O | Red LED (Fault Alert) | Visual Logic Output Status[cite: 1] |

## ⚙️ Setup and Installation
### Host PC Setup
1. Clone the repository to your workspace.
2. Ensure Python 3.x is configured on your machine alongside `pyserial` dependencies[cite: 2].
3. Run `python Software_Host/setup_db.py` to compile the baseline SQLite relational schemas from your master census payload[cite: 3].
4. Launch the executive control layer panel: `python Software_Host/evm_controller.py`[cite: 2].

### Hardware Setup
1. Compile and deploy the `Firmware/evm.ino` source package to target the ESP32 platform[cite: 1].
2. Ensure an SD flash peripheral formatted to FAT32 is present in the onboard deck[cite: 1].
3. Bridge hardware nodes explicitly as detailed in the I/O multiplexer constraints map[cite: 1].
