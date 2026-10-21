# Isolated serial interface: wiring on perfboard (3.3V ESP side)

Reference for building the optocoupler board for an ESP32 node. It replaces the
5V Arduino side of `Kamstrup_isolated_IO.png`. Same two EL817 channels, same
meter side. Only the ESP-side rail changes: **3V3 instead of 5V**.

## What the existing board is

Photo: `Kamstrup_TTL_interface_arduino.png` (in place on the meter:
`Kamstrup_Energymeter_full.png`). It is a small pad-per-hole perfboard with:

- two 4-pin optocouplers in the middle (the two EL817),
- three or four resistors around them (the meter-side 820 ohm and 10k, the ESP-side 680 ohm and 4.7k from the schematic),
- one white insulated jumper wire (the link between the two Vcc points),
- a bundle of coloured wires going out, plus tinned solder bridges along the edges.

The part count matches the schematic. The copper tracks are not readable from the
photo, so **check the real wiring with a multimeter (continuity)** against the
netlist below before trusting it.

The schematic has **no pulse channel**, only serial. See "Pulse" below.

## Signal polarity

Both channels are **non-inverting**: idle stays high on each side, a start bit
(low) gives low on the other side. No inverter is needed on RX or TX, and no
`invert` on the ESP UART. (The earlier note that RX was "likely inverted" was
over-cautious.)

## Netlist

EL817 pins: 1 anode, 2 cathode (LED side). 3 emitter, 4 collector (transistor side).

Meter side (no shared ground with the ESP side):

| From                | To                                |
|---------------------|-----------------------------------|
| meter pin 6 (Vcc)   | R1 (820R) -> U1 pin 1             |
| meter pin 2 (Data out) | U1 pin 2                       |
| meter pin 1 (GND)   | U2 pin 3                          |
| meter pin 5 (Data in) | U2 pin 4 and R4 (10k) -> meter pin 6 (Vcc) |

ESP side:

| From            | To                                         |
|-----------------|--------------------------------------------|
| ESP GND         | U1 pin 3                                   |
| ESP RX GPIO     | U1 pin 4 and R2 (4.7k..10k) -> ESP 3V3      |
| ESP 3V3         | R3 (680R) -> U2 pin 1                      |
| ESP TX GPIO     | U2 pin 2                                   |

R2: 4.7k (as in the schematic image); 10k also works at 300 baud.

## Schematic

```
 METER SIDE (isolated)           | barrier |            ESP SIDE (3.3V)

 Channel A: meter -> ESP         U1 EL817
 pin 6 Vcc --[R1 820R]-- 1 +-----+ 4 --+---------------- ESP RX GPIO
                         LED |  >  | transistor         |
 pin 2 Data out -------- 2 +-----+ 3 --- ESP GND       [R2 4.7k]
                                                        |
                                                       3V3

 Channel B: ESP -> meter         U2 EL817
                                                       3V3
 pin 6 Vcc --[R4 10k]--+                                |
                       |                              [R3 680R]
 pin 5 Data in --------+-- 4 +-----+ 1 ----------------+
                  transistor |  <  | LED
 pin 1 GND -------------- 3 +-----+ 2 ---------------- ESP TX GPIO
```

## Placement on the board

Meter side on the left, ESP side on the right, a clear gap in the middle. Each
EL817 straddles the gap (LED side on one side, transistor side on the other).
Keep the gap free of tracks and solder bridges.

```
   METER SIDE            gap            ESP SIDE
 +--------------------+-------+-----------------------+
 | pin6 Vcc  R1 ----- | U1    | ---- R2 --- 3V3       |
 | pin2 Data out ---- | (A)   | ---- RX GPIO          |
 |                    |       | ---- GND              |
 |                    |       |                       |
 | pin1 GND --------- | U2    | ---- TX GPIO          |
 | pin5 Data in --+-- | (B)   | ---- R3 --- 3V3       |
 | pin6 Vcc --R4--+   |       |                       |
 +--------------------+-------+-----------------------+
   U2 is rotated 180 degrees from U1 (LED on the ESP side).
```

Ground rule: ESP GND only touches U1 pin 3 and the ESP. Meter GND only touches
U2 pin 3 and the meter. Never join them.

## Does it change with DevKit C or C3 Super Mini?

The circuit does not change. Both boards are 3.3V logic, so the same board works
with either. Use the board's **3V3 pin** for R2 and R3 (not 5V: an LED anode at
5V against a 3.3V GPIO would never switch fully off).

| Item        | DevKit C V4                  | C3 Super Mini                         |
|-------------|------------------------------|---------------------------------------|
| RX / TX     | GPIO16 / GPIO17 (UART2)      | GPIO5 / GPIO6                         |
| Pulse       | GPIO4                        | GPIO4                                 |
| Boot noise  | none on 16/17                | none (UART0 pins GPIO20/21 avoided)   |

On the C3 the boot ROM prints on GPIO21 at power-up. That would go into the
meter's Data in, so the meter UART uses GPIO5/GPIO6 instead. Avoid the strapping
pins 2, 8 and 9.

## Pulse (not in the schematic)

The meter has an S0 output (label: `S0 = 1000 imp/kWh`, terminals 20/21, the blue
block in the photo; unconfirmed that this is what the old setup used). S0 is an
isolated, passive transistor output, so it needs **no extra optocoupler**:

- S0 `+` (collector) -> pulse GPIO (GPIO4, internal pull-up)
- S0 `-` (emitter) -> ESP GND

Check the `+`/`-` marking and test with a multimeter. The alternative is a third
EL817 channel from module pin 3, wired like channel A.

## Parts

- 2x EL817 (DIP-4)
- R1 820R, R3 680R, R4 10k, R2 4.7k (or 10k), 1/4 W
- perfboard, about 10 x 8 holes
- wire to the meter module (pins 1, 2, 5, 6) and to the ESP (3V3, GND, RX, TX)

## Bench check before connecting the ESP

1. Continuity between the two sides must be open (meter GND to ESP GND, meter Vcc to 3V3).
2. With the meter connected, probe U1 pin 4 to ESP GND: it should sit near 3.3V and
   drop low in bursts when the meter answers `/?!`.
3. R1 is 820R, not 680R: at 3.6V it drives about 2.9 mA through the LED, inside
   the meter's 3 mA pin limit (680R would give about 3.5 mA).
