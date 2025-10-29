# RP2350 HUB75E Driver

This repository contains the firmware and hardware assets for an RP2350-based HUB75E LED matrix driver. The goal is a small, efficient driver for HUB75E-compatible LED panels with a clear separation between hardware and software artifacts.

## Features

- RP2350 MCU-based driver (firmware lives in `SW/`).
- Designed for HUB75E LED matrix panels (standard row/column control signals).
- Double-buffering and timing-friendly design (firmware should use DMA/PWM where available).
- Configurable panel dimensions and scan modes (single/double/quad scan typically supported by driver code).
- Example pinout and wiring guidance included below (adjust to your PCB/board layout).

## Suggested pinout (example)

Below is an example mapping of HUB75E signals to RP2350 GPIOs. This is a suggested starting point — adapt the mapping to match your board and wiring.

Signals:
- R1, G1, B1 — top half color data
- R2, G2, B2 — bottom half color data
- A, B, C, D, E — row address lines (E optional for taller panels)
- LAT (latch), OE (output enable), CLK (clock)

Example mapping (adjustable):

```
HUB75E -> RP2350 (example)
R1  -> GP0
G1  -> GP1
B1  -> GP2
R2  -> GP3
G2  -> GP4
B2  -> GP5
CLK -> GP6
LAT -> GP7
OE  -> GP8
A   -> GP9
B   -> GP10
C   -> GP11
D   -> GP12
E   -> GP13   # only if your panel uses E
GND -> GND
5V  -> 5V (panel power) / use separate power domain for LEDs
```

Notes:
- Keep LED panel power (5V) separate from RP2350 3.3V supply and route grounds together.
- Use level shifting for data lines if required by your panel voltage (many HUB75 panels accept 3.3V logic, verify for your display).
- Use short clock/latch traces and proper decoupling near the MCU and panel connectors.

## File structure

```
/
├── README.md           # (this file)
├── main.py             # project entry or examples (if present)
├── requirements.txt    # Python requirements (if any tooling is used)
├── SW/                 # Firmware and software for RP2350
│   └── README.md       # notes about building firmware and examples
├── HW/                 # Hardware files: schematics, PCB, BOM
│   └── README.md       # notes about hardware files and BOM
├── pmod/               # documentation / extra materials
└── ...
```

Place firmware sources, build instructions and examples in `SW/`. Place KiCad/EAGLE/etc files, BOM, and fabrication notes in `HW/`.

## Usage (quick)

1. Wire the panel to the RP2350 following the chosen pinout. Verify 5V panel power and common ground.
2. Build or copy firmware into the RP2350 (see `SW/README.md` for build steps — typical workflows: CMake/Make, or MicroPython deployment).
3. Configure panel parameters in firmware (width, height, scan mode, color depth).
4. Power-up panel and MCU. Start firmware and verify rows/columns drive as expected. Begin with low brightness for safety.

## Notes and troubleshooting

- If colors are scrambled, verify the data pin order and panel wiring (R/G/B mapping).
- If the panel is dim or blank, check OE (output enable) wiring and power distribution.
- For best results use DMA and hardware-timed PWM where available to reduce CPU jitter.

## License

See repository LICENSE file for licensing information.

