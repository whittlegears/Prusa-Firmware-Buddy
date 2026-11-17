# ARTME 3D screw extruder as the Core One print head

This fork lets a Prusa Core One print with an ARTME 3D Desktop Filament Extruder MK3
mounted in place of the nozzle, so shredded plastic goes straight into the part with
no filament step.

The two machines keep their own controller boards. The Core One moves the axes, heats
the bed and probes with the load cell. The ARTME board (MKS Gen L) heats the barrel and
turns the screw. A three-wire data cable joins them.

**Status: compiles, never run on hardware.** Treat the first power-up as a bench test,
with the screw empty and a hand on the ARTME power switch.

## How it works

```
Core One mainboard --(internal bus)--> xBuddy extension board --(MMU port, 115200 baud)--> ARTME MKS Gen L
```

Ten times a second the printer sends `M951 S<screw rpm> T<barrel target> K<check>` and
the extruder answers `PH T:<barrel temp> /<target> R:<rpm> F:<flags>`.

- **Temperature.** The printer's nozzle temperature *is* the ARTME barrel temperature.
  Setting the nozzle temperature on the printer (screen, G-code, PrusaLink) sets the
  barrel target. The printer's own nozzle heater output is forced off.
- **Extrusion.** The printer's extruder motor output is not connected to anything. The
  firmware watches how much filament the E axis would have fed, converts it to melt
  volume (1.75 mm filament) and asks the screw for the speed that delivers it.
  Retractions up to 5 mm are ignored, because a screw cannot pull melt back.
- **Safety.**
  - Printer stops hearing from the extruder for 3 s while a temperature is set: red
    error screen, everything on the printer stops.
  - Extruder stops hearing from the printer: screw stops after 1.5 s, barrel heater
    turns off after 10 s and the extruder returns to stand-alone mode.
  - A garbled command is rejected by its check value.
  - The extruder never accepts more than its own limits (25 rpm, 260 °C).

## Parts of this repository

| Path | Runs on | What changed |
| --- | --- | --- |
| `artme/Desktop-Filament-Extruder-MK3/` | ARTME MKS Gen L | ARTME's MK3 firmware (main branch, commit `348eca6`) plus `PRINTHEAD_LINK`: the `M951` command, link on Serial1, timeouts. Stand-alone filament making still works when no printer is talking to it. |
| `src/puppy/xbuddy_extension/artme.*` | xBuddy extension board | Passes commands and answers between the internal bus and the MMU port. |
| `src/feature/artme_link/` and hooks in Marlin | Core One mainboard | Temperature from the link, E motion to screw speed, `M951` G-code, error handling. Only with `ARTME_LINK=ON`. |

## Wiring

Three wires, **data and ground only**. Do not connect any power pin of the MMU port.

| xBuddy extension MMU port | MKS Gen L |
| --- | --- |
| TX | D19 (RX1), signal pin of the `Z+` endstop connector |
| RX | D18 (TX1), signal pin of the `Z-` endstop connector |
| GND | GND |

Check the MMU port pinout against Prusa's board documentation before connecting; it is
not recorded in this repository. An MMU3 cannot be used at the same time.

On the printer, keep the Nextruder's board, load cell, fans and heatbreak thermistor
connected. Unplug the Prusa nozzle heater and extruder motor.

## Building and flashing

Core One (also builds and bundles the extension board firmware, which the printer
flashes by itself):

```
python utils/build.py --preset coreone --bootloader yes -D ARTME_LINK=ON
```

The firmware is unsigned, so the printer needs its appendix broken to accept it, as
for any custom Prusa firmware.

ARTME: open `artme/Desktop-Filament-Extruder-MK3/MK3/MK3.ino` in the Arduino IDE and
upload to "Arduino Mega 2560" over USB, as in ARTME's own instructions.

## First run

1. Power both. The ARTME display shows "Printer link active" once commands arrive.
   `M951` on the printer reports the link state.
2. Set a nozzle temperature on the printer and watch the ARTME barrel follow it. The
   ARTME motor switch has to be on for the screw to turn.
3. Calibrate the screw: extrude a known E length into air, weigh it, and set the melt
   volume per revolution with `M951 V<mm3>` (stored permanently). The default of
   300 mm³ per revolution is a placeholder, not a measurement. Raise it if the printer
   over-extrudes.

## Known limits

- Screw response is slow next to a filament drive. Expect to need low speeds, low
  accelerations and no retraction; pressure advance has no meaning here.
- The printer's selftest steps for the nozzle heater and extruder will fail. Skip them.
- The filament sensor in the Nextruder sees no filament; turn it off in the printer
  settings.
- The ARTME safety cooldown still applies: barrel hot with the screw stopped for
  50 minutes halts the extruder, which the printer then reports as a lost link.
- A nozzle target above 260 °C is cut to 260 °C by the extruder, so the printer would wait
  for a temperature that never comes. Keep targets at or below 260 °C.
- The barrel heat-up check on the printer expects at least 2 °C rise every 120 s.
