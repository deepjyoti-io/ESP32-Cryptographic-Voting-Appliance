# Secure Dual-Tier Electronic Voting Machine (EVM)

## 📌 Project Overview
This project implements a secure, hardware-in-the-loop Electronic Voting Machine system. It utilizes a two-tier architecture: an **ESP32-based embedded voting terminal** and a **Python-based administrative host dashboard**. The system ensures vote integrity through cryptographic hashing, non-volatile memory redundancy, and secure asynchronous serial authorization.

## 🛠️ Core Technologies & Tech Stack
* **Microcontroller:** ESP32 (C/C++ via Arduino Core)
* **Host Software:** Python 3 (Tkinter, PySerial)
* **Database:** SQLite3 (RDBMS)
* **Hardware Peripherals:** OLED (SSD1306/SH1106 via Software SPI), SD Card Module (Hardware SPI), Mechanical Switches.
* **Security & Data Integrity:** `mbedtls` SHA-256 Hashing, ESP32 NVS (Non-Volatile Storage).

## 🚀 Key Engineering Features

### 1. Finite State Machine (FSM) Architecture
The embedded firmware is driven by a robust Finite State Machine (`EVMState: LOCKED, AUTHORIZED, CANDIDATE_SELECTED, STORE_VOTE, COMPLETE, SYS_ERROR`) ensuring the hardware can only accept inputs during authorized windows, preventing double-voting and race conditions.

### 2. Cryptographic Audit Trail
To prevent tampering, every recorded vote generates a payload containing the current tally, a timestamp, and the previous vote's hash. This is processed using the `mbedtls` library to generate a continuous **SHA-256 hash chain** logged directly to the SD card.

### 3. Asynchronous UART Authorization Protocol
The physical voting terminal remains completely locked until it receives a specific, formatted packet (`AUTH|VoterID|CRC`) over the Serial interface from the Python host. The host application queries a local SQLite database to verify voter eligibility before transmitting the authorization token.

### 4. Memory Redundancy & Fault Tolerance
Votes are simultaneously written to the ESP32's internal Non-Volatile Storage (NVS) and external SD card storage. If an SD card failure is detected during the `STORE_VOTE` state, the system safely halts execution (`SYS_ERROR`) and triggers a hardware alert.

### 5. Hardware Debouncing & Multi-Press Protection
Inputs are safeguarded with software-defined debounce delays and simultaneous multi-button-press rejection logic to ensure absolute voter intent accuracy.

## ⚙️ Setup and Installation
### Host PC Setup
1. Clone the repository.
2. Ensure Python 3.x is installed along with `pyserial`.
3. Run `python setup_db.py` to initialize the SQLite database from your voter CSV.
4. Launch the dashboard using `python evm_controller.py`.

### Hardware Setup
1. Flash the `evm.ino` firmware to an ESP32.
2. Ensure the SD card is formatted to FAT32.
3. Connect the hardware according to the defined SPI and GPIO pinouts.
