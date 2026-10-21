# Optocoupler wiring notes (3.3V ESP side)

The existing 2×EL817 isolation circuit (`docs/Kamstrup_Arduino_Isolated.drawio`,
`docs/Kamstrup_isolated_IO.png`) is reused. Only the ESP side changes from 5V to
3.3V. This file is a reference for the electrical review — it is not auto-built.

## Golden rules

- **Separate supplies.** The meter side (LED side of the optocouplers) and the
  ESP side (transistor side) must have **no shared ground**. Coupling is only
  through the optocoupler light barrier. Sharing ground defeats the isolation
  Kamstrup requires.
- **Do not power the ESP from meter pin 6.** Pin 6 supplies 3.6V at ~1 mA avg
  (5 mA max) — 50–300× too little for an ESP32. Use an external 5V supply
  (USB adapter or a 230V→5V module such as a Hi-Link HLK-PM01 in the enclosure).
- **ESP32 GPIOs are 3.3V, not 5V tolerant.** The transistor-side pull-ups go to
  **3.3V**, never 5V.

## Meter side (unchanged, sized to the 3.6V / 3 mA spec)

- Pin 1: GND (meter reference)
- Pin 2 "Data out"  → LED of the RX optocoupler, 820Ω series (meter → ESP)
- Pin 3 "Pulse out" → LED of the pulse optocoupler, 820Ω series (meter → ESP)
- Pin 5 "Data in"   ← driven from the ESP-side optocoupler (ESP → meter)
- Keep meter-side series/pull resistors as in the existing schematic.

## ESP side (changed: 3.3V)

- Optocoupler emitter → ESP GND (ESP-side reference).
- Optocoupler collector → ESP input GPIO **and** a pull-up to **3.3V**.
- Lower the pull-up from 10kΩ to ~**4.7kΩ** for a healthier collector current
  at 3.3V (depends on the EL817 CTR). 4.7kΩ gives ~0.7 mA, comfortably within
  the EL817's range while keeping edges fast enough for 300 baud and the pulse.

## Pin mapping (matches the YAML board packages)

| Meter pin | Signal              | DevKit C V4 | C3 Super Mini |
|-----------|---------------------|-------------|---------------|
| 2         | Data out (→ ESP RX) | GPIO16      | GPIO5         |
| 5         | Data in  (← ESP TX) | GPIO17      | GPIO6         |
| 3         | Pulse out (→ ESP)   | GPIO4       | GPIO4         |

## Signal polarity

Both serial channels are **non-inverting** (idle high on both sides), so the
UART needs no inversion. See `pcb-wiring.md` for the netlist and layout.

- **Pulse:** the schematic has no pulse channel. If the pulse comes from the S0
  terminals, wire S0 `+` to the pulse GPIO and S0 `-` to ESP GND. If pulses don't
  register, set `inverted: true` on the `pulse_pin` in `common/kamstrup-base.yaml`
  (the line is present, commented).
