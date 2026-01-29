# AG_OTA Project

## 📌 Overview
**AG_OTA** is a Bluetooth Low Energy (BLE) based **Over-The-Air (OTA) firmware update system** integrated with a custom **AG Service**. The project is designed with modularity in mind so that core logic can remain reusable across platforms while the hardware- or OS-specific layers are isolated.

The project supports:
- BLE scanning connect target MAC_ID
- Discover AG BLE service
- MTU Exchange 
- Secure OTA firmware transfer
- Notification-based data exchange
- `agota.c` and `agota.h`Cross-platform 

---

## 🧱 Architecture

```
AG_OTA/
├── include/          # Public headers
│   └── agota.h
│
├── src/              # Source files
│   ├── agota.c
│   └── main.c
│
├── CMakeLists.txt
├── prj.conf
└── README.md
```

---

## 🔧 Features

### BLE AG Service
- Custom 128-bit Service UUID
- Read / Write / Notify characteristics
- Used for application-level data exchange
```

Services and their UUIDs:
Service UUID: 00001800-0000-1000-8000-00805f9b34fb
  ├── Characteristic UUID: 00002a00-0000-1000-8000-00805f9b34fb  prop ['read']
  └── Characteristic UUID: 00002a01-0000-1000-8000-00805f9b34fb  prop ['read']

Service UUID: 00001801-0000-1000-8000-00805f9b34fb
  └── Characteristic UUID: 00002a05-0000-1000-8000-00805f9b34fb  prop ['indicate']

Service UUID: 0000180a-0000-1000-8000-00805f9b34fb
  ├── Characteristic UUID: 00002a29-0000-1000-8000-00805f9b34fb  prop ['read']
  ├── Characteristic UUID: 00002a24-0000-1000-8000-00805f9b34fb  prop ['read']
  ├── Characteristic UUID: 00002a26-0000-1000-8000-00805f9b34fb  prop ['read']
  └── Characteristic UUID: 00002a27-0000-1000-8000-00805f9b34fb  prop ['read']

Service UUID: d6f1d96d-594c-4c53-b1c6-144a1dfde6d8
  ├── Characteristic UUID: 7ad671aa-21c0-46a4-b722-270e3ae3d830  prop ['read', 'write', 'notify']
  └── Characteristic UUID: 23408888-1f40-4cd8-9b89-ca8d45f8a5b0  prop ['write']

Service UUID: 000000ff-0000-1000-8000-00805f9b34fb
  └── Characteristic UUID: 0000ff01-0000-1000-8000-00805f9b34fb  prop ['read', 'write', 'notify']

```
---
## 🔁 OTA Flow (High Level)

1. BLE Connects
2. MTU Exchange
3. Service discovery
4. Enable Notification 
5. Packet size transfer
6. OTA Start Command and wait for response
7. Firmware transferred in chunks ()
8. OTA Stop Command and wait for response
9. AG will reboot and Upgrage it's firmware

![OTA PROCESS](additional\ota_process.png)

---

## 🖥️ Supported Platforms

### Embedded Targets
- Zephyr RTOS
- Nordic nRF52 / nRF54 series
- ESP32 (partial support)

---

## 🚀 Getting Started

### Embedded Side
1. Configure BLE services in `prj.conf`
2. Enable notifications for AG & OTA characteristics
3. Build and flash firmware

### Uplode AG firmware in NRF memory

```bash
>>> nrfjprog --family UNKNOWN --recover
>>> arm-none-eabi-objcopy -I binary -O ihex --change-addresses 0x000B6000 weware_ag_v153.bin agfw.hex
>>> nrfjprog --program "C:\Users\VasuJain\Documents\Sumit\wheelseye\AG_OTA\build\agfw.hex" --verify
```

---

## 🔐 Security Notes
- OTA should be enabled only in secure mode
- Production builds should:
  - Disable OTA after update
  - Validate firmware signature
  - Use encrypted BLE connections

---

## 🧪 Testing

- uploade this fw in NRF54l15 **Unity + CMock**
- OTA ch **Unity + CMock**
- OTA chunk handling verified with boundary conditions

---

## 📄 Coding Standards
- MISRA-inspired C style
- No dynamic memory allocation in embedded code
- Clear separation of platform-dependent logic

---

## 👤 Ownership & Maintenance

- **Author / Owner:** Sumit Paropkari
- **Project:** AG_OTA
- **Created:** Jan 2026

---

## 📜 License

This project is **proprietary and confidential**.

Unauthorized copying, modification, distribution, or use of this software, in whole or in part, is strictly prohibited without prior written permission from the owner.

---

## 📬 Contact

For questions, issues, or collaboration requests:

**Sumit Paropkari**  
Embedded Systems Developer

---

> _AG_OTA is designed to be portable, testable, and production-ready._

