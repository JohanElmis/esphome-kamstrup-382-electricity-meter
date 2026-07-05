# Kamstrup 382 – ESP32/ESPHome Meter Node

## 1. Background and purpose

5 Kamstrup 382 electricity meters are installed on sub-distribution boards in the house, measuring consumption per branch circuit. Each meter has:

- An **IEC 61107 mode A serial interface** (300 baud, 7 data bits, even parity, 1 stop bit) for historical/cumulative data (energy register, tariffs, hour counter, peak power, etc.)
- A **pulse output, 1000 imp/kWh**, for instantaneous power

Previous solution: Arduino (serial + pulse counting via interrupt) → optical isolation → Raspberry Pi (network connectivity).

**Goal of the new solution:** replace the Arduino + RPi chain with **one standalone ESP32 node per meter**, which itself handles:

1. Instantaneous power reading via pulse counting
2. Periodic/on-demand reading of historical data via serial IEC61107 request
3. Publishing to MQTT on change (absolute threshold; a percentage threshold would also be acceptable, but absolute is sufficient)
4. A local web interface
5. A Prometheus `/metrics` endpoint

## 2. Chosen platform

**ESPHome** on ESP32 (e.g. an ESP32-DevKitC or similar), one node per meter.

Rationale:
- Built-in support for `pulse_meter` (instantaneous power + energy via an S0-like pulse input)
- Built-in support for `uart` with configurable baud rate/data bits/parity/stop bits
- Built-in support for `mqtt`, `web_server` (v2/v3), and the `prometheus` component
- The only missing piece is IEC61107 protocol parsing → solved with a custom component/lambda in C++ embedded in the ESPHome YAML

Only the IEC61107 part (request/response parsing) needs to be hand-written; everything else is configured declaratively.

## 3. Architecture

- **One ESP32 per meter** (5 nodes in total), no central Raspberry Pi required
- Each node connects independently to WiFi and the MQTT broker
- Prometheus scrapes all 5 nodes as separate targets, tagged per sub-board (e.g. `meter="UC1"` … `meter="UC5"`)
- Advantages over a shared/multiplexed solution: simpler code, no risk of one crashing node taking down the others, and the ESP32 has hardware PCNT units for jitter-free pulse counting

## 4. Electrical / hardware

### 4.1 Power supply – IMPORTANT

**The Kamstrup interface's Pin 6 (Supply) CANNOT power an ESP module.**

Spec: 3.6V, average current 1 mA (averaged over 60 s), max 5 mA.

An ESP32 typically draws 20–80 mA when active (WiFi idle) and 150–300+ mA during WiFi transmission (peaks) — a factor of 50–300x too much for the meter's interface.

→ **External power supply is required for the ESP side**, e.g.:
- A 5V/1A USB wall adapter, or
- A built-in 230V→5V module (e.g. Hi-Link HLK-PM01) in the same enclosure, if 230V is available at the sub-board (usually the case)

### 4.2 Galvanic isolation remains a requirement

Kamstrup requires optocouplers between the meter and any external interface ("Optocouplers have to be used where interfacing with the meter"). The existing circuit with **2× EL817** (existing schematic) is reused as the base:

- **Meter side** (680Ω/10kΩ against the meter's pins): unchanged — sized to the meter's 3.6V/3mA spec, not to the microcontroller
- **ESP side**: change Vcc from 5V → **3.3V** (ESP32 GPIOs are not 5V-tolerant on most pins). Consider lowering the pull-up from 10kΩ to e.g. **4.7kΩ** for a safer collector current at the lower voltage (depending on the EL817's CTR)
- **Do not share ground between the meter side and the ESP side** — that would defeat the isolation. The meter side and ESP side should have completely separate power supplies and references, coupled only via the optocouplers' light barrier

### 4.3 Pins on the Kamstrup module interface (ref. Kamstrup 382 spec, valid for 5550-457 rev G3 – 5550-599 rev B1)

| Pin | Function | Details |
|-----|----------|---------|
| 1 | GND | Reference |
| 2 | Data out | Serial data from the meter. Active high, internal 100kΩ pull-up. Max 3.6V, min 0V. Active low max 3mA |
| 3 | Pulse out | 1000 imp/kWh. Same electrical spec as pin 2 |
| 4 | Pulse in | Tariff control/pulse receiving. Max 3.6V |
| 5 | Data in | Serial data to the meter. Max 3.6V |
| 6 | Supply | 3.6V, average 1mA (60s average), max 5mA. Short pin 1+4 for tariff change |

## 5. Protocol: IEC 61107 mode A (to be implemented)

- **Request to meter:** `/?![CR][LF]`
- **Response:** `/KAM 685-X3X2X1-Y2Y1-Z2Z1[CR][LF]` followed by `[STX]` + data records (each line `OBIS-code(value*unit)[CR][LF]`) + `[ETX][BCC]`
- **Response time:** 20–600 ms after request
- **BCC (block check character):** 7-bit addition of every character from (but not including) `[STX]` up to and including `[ETX]`
- **Data records to extract:**
  - `0.0(########)` – customer/serial number
  - `1.20(#######*kWh)` – total energy register
  - `1.20.1(#######*kWh)` – tariff 1
  - `1.20.2(#######*kWh)` – tariff 2
  - `1.31(#######*h)` – hour counter
  - `1.26(#######)` – pulse input
  - `1.6(######,#*kW)` – current peak power
  - `1.6*1(######,#)!` – previous month's peak

A full sample response is included in the Kamstrup spec (type number `685-382-OK-15` etc.).

## 6. Functional requirements for the implementation

### 6.1 Instantaneous power (pulse counting)
- ESPHome `pulse_meter` platform on the pulse output (pin 3)
- Configure 1000 imp/kWh → automatic calculation of instantaneous power (kW) based on time between pulses
- Cumulative energy can be derived in parallel if desired

### 6.2 Historical data (serial)
- `uart` component: 300 baud, 7 data bits, even parity, 1 stop bit
- Custom component (C++/lambda) that:
  1. Sends `/?!\r\n`
  2. Buffers incoming data until `[ETX]` + BCC has been received (with a timeout, e.g. 1–2 s)
  3. Validates the BCC
  4. Parses out the OBIS fields above using simple string splitting/regex
  5. Exposes each field as an ESPHome `sensor`/`text_sensor`
- Read interval: configurable, e.g. every 5–15 minutes (historical data doesn't change quickly)

### 6.3 MQTT
- Publish each sensor on change
- Filter: **absolute difference threshold** (ESPHome `filters: - delta: X`) — sufficient per requirements, no percentage calculation needed
- Threshold values per sensor defined in config (e.g. power: 0.1 kW; energy register: 1 kWh)
- Suggested topic structure: `kamstrup/<meter_id>/<field>`

### 6.4 Web interface
- ESPHome built-in `web_server` (v3), exposes a local dashboard with current sensor values per node

### 6.5 Prometheus
- ESPHome built-in `prometheus` component → `/metrics`
- Each node becomes its own scrape target; tag per sub-board in the Prometheus `scrape_configs` (`meter: "UC1"` etc.)

## 7. Open decisions / to confirm during the Claude Code session

- [ ] Exact ESP32 model/dev board to use
- [ ] MQTT broker address, authentication, topic naming convention
- [ ] Prometheus threshold (delta) values per sensor
- [ ] Read interval for historical serial data
- [ ] If Home Assistant is already in use in the house — possibly use the ESPHome native API towards HA instead of/in addition to standalone MQTT+Prometheus
- [ ] Physical mechanics: shared enclosure with a 230V→5V module, or USB power per node

## 8. Deliverables to build in Claude Code

1. **ESPHome YAML skeleton** per meter node (uart, pulse_meter, mqtt, web_server, prometheus), parameterized so the same template can be reused for all 5 nodes (only `meter_id`/pins/calibration differ)
2. **Custom C++ component** for IEC61107 request/response parsing incl. BCC validation, exposed as an ESPHome external component
3. **Delta filter configuration** per sensor for MQTT publishing
4. **Prometheus scrape config example** for all 5 nodes
5. (Optionally) **KiCad/schematic reference** for the updated EL817 circuit with a 3.3V ESP side

---

*Sources: Kamstrup 382 communication interface specification (2004-06-11), custom circuit sketch with 2×EL817 optical isolation.*
