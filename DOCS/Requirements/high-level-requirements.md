# High-Level Design (HLD) Document

## 1. Project Overview

**Project Title:** Picket Module

**Description:** A microcontroller-based system that monitors the status of doors and cabinets. It will be used to make the high level system contextually aware of which doors and or cabinets are currently open or closed.

**Objective:**
To design and implement a reliable, low-power, and cost-effective microcontroller-based system for door and cabinet status monitoring that can operate autonomously and send data over the CAN bus.

---

## 2. Microcontroller Selection

**Selected Microcontroller:**
**ESP32-S3** (Waveshare ESP32-S3-RS485-CAN)
[Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

**Rationale for Selection:**
- Off-the-shelf board with onboard CAN transceiver (TJA1051), buck converter (7-36V), and 20-pin header for sensor connections.
- 13 GPIOs available on pin header with internal pull-ups — no external resistors needed.
- Strong ecosystem and ESP-IDF development support.
- Shared board with Solstice module, reducing BOM complexity.

---

## 3. System Requirements

### 3.1 Functional Requirements

| Requirement | Description |
|------------|-------------|
| Door Sensing | Read and send over CAN bus the door or cabinet status |
| Data Transmission | Send collected data to the rest of the network via CAN protocol |
| Power Management | Support for low-power modes to reduce load on installed environment |

### 3.2 Non-Functional Requirements

| Requirement | Description |
|------------|-------------|
| Reliability | System must operate continuously without disruption indefinitely for solar installs |
| CAN Bus Data Usage | **Needs calculation to see what is available on a 500k CAN Bus.** |
| Environmental Tolerance | Operate within -20°C to +70°C |
| Cost | Target cost < $5 per unit |
---

## 4. System Architecture Overview

### 4.1 Hardware Components

| Component | Description |
|----------|-------------|
| ESP32-S3 (Waveshare ESP32-S3-RS485-CAN) | Main microcontroller with onboard CAN transceiver |
| Reed Swith | Any brand should suffice, justt needs to provide an on/off state to trigger correctly. |

### 4.2 Software Components

| Component | Description |
|----------|-------------|
| Main Application | Main data collection, and transmission |
| Sensor Drivers | Interface with reed switches |
| Communication Stack | CAN Bus coomunication implementation |
| OTA | Over the air update capabilities with trigger coming from CAN Bus. |

---

## 5. Communication Protocol

- **CAN Bus:** CAN Bus protocol used to transmit data  
- **Data Format:** Binary representation of each sensor status
- **Transmission Frequency:** 5 times per second to avoid delayed notification of doors changing status during travel.
---

## 6. Power Management Strategy

- **Battery:** Connected to house battery of implemented vehicle.
- **Power Supply Circuit:** 5V DC to DC buck converter with 3.3V regulator (e.g., AMS1117)  
---

## 7. Development Tools and Environment

| Tool | Description |
|------|-------------|
| ESP-IDF | Development framework (v5.5.2) |
| VS Code | Development IDE |
| KiCAD | EDA Design tool for schematic and PCB |
| FreeCAD | CAD tool used for enclosure design |
| Version Control | Git with GitHub for repo and source control |
| Documentation Tools | Markdown |

---

## 8. Testing and Validation

| Test Type | Description |
|----------|-------------|
| Unit Testing | Test individual modules (e.g., sensor drivers, communication stack) |
| Integration Testing | Ensure all components work together as expected |
| Performance Testing | Measure power consumption, response time, and data accuracy |
---

## 9. Future Enhancements

- Add GPS module for location tracking  
- Support for multiple sensors (e.g., PM2.5, CO2)  
- Edge computing for local data processing  
- Integration with IoT platforms (e.g., AWS IoT, Azure IoT Hub)  
- Mobile app integration for real-time monitoring

---