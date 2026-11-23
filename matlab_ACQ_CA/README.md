# matlab_ACQ_CA: EMG acquisition, calibration and control (MATLAB)

The MATLAB side of the EMG car. It reads two EMG channels from the Uno, calibrates the user, runs
one of Kirby's control algorithms, turns its output into a drive command, and sends that to the Nano,
which radios it to the car.

```
Uno (A0, A1) --USB--> parse -> volts -> Kirby's controller (0..1 per channel) -> mapping -> 1 byte --USB--> Nano --nRF--> STM32
```

## Quick start

1. Flash the boards (see `docs/uno_spikershield_nrf_plan.md`):
   - **Uno:** `arduino_emg_acquisition/full_2_channel_wireless_ctrl_alg`
   - **Nano:** `arduino_emg_acquisition/nrf_tx_drive`
   - **STM32:** `demo_full_PC_car_ctrl()`
2. Plug both boards into the laptop. **Close every Arduino Serial Monitor.** Run the laptop on
   battery while electrodes are on someone.
3. In MATLAB:
   ```matlab
   cd('...\emg_car_repo\matlab_ACQ_CA')
   run_emg_car                    % lists the COM ports
   run_emg_car("COM5", "COM7")    % Uno port first, then Nano port
   ```
4. Follow the menus in the Command Window:
   1. **View live signals?** Optional; checks the electrode hookup.
   2. **Calibration:**
      - 1 = new, which records 5 s of rest, then N × (2 s contract, 2 s relax) per channel, shows the
        review plots and lets you redo channels.
      - 2 = load a saved one.
      - 3 = revise a saved one.
   3. **Controller** and **mapping**: press Enter for the defaults.
   4. **Drive LIVE?** Answer `n` first for a **dry run**: everything runs and is plotted, but only
      E-STOP is sent.
   5. Close the drive window to stop. Then choose: drive again, change controller or mapping,
      recalibrate, view signals, or quit.

### The drive window
- **Top plot:** raw EMG in volts, last 5 s.
- **Middle plot:** controller output (0–1) per channel, plus the dashed **activation level** line.
  The title shows what the mapping wants to send.
- **Sliders:** Kirby's per-channel **Sensitivity**, **Speed** and **Baseline** (only the ones the
  selected controller uses are enabled), plus the mapping's **Activation level**.
- **E-STOP button or spacebar:** stops the car and keeps it stopped until **RESUME** is clicked.
  **LIVE mode starts stopped:** click RESUME to drive.
- The car also stops on its own if the Uno stops sending data (0.25 s), if MATLAB stops sending
  (200 ms, the STM32 failsafe), and when you close the window or press Ctrl+C.
- At the end of each drive, the Command Window prints the longest gap between commands. Over ~150 ms
  means this laptop draws too slowly; see "Tuning" below.

## Files

| File | What it is | From Kirby's code |
|---|---|---|
| `run_emg_car.m` | **Start here.** Opens both ports once, runs the menus, sends E-STOP on any exit | Replaces running sections 0A–2 by hand |
| `emg_calibrate.m` | New, load or revise calibration; review plots; save to `saved_data/` | Sections 1A + 1B |
| `compute_calibration_channel.m` | Per-channel calibration math: threshold, rest RMS, contraction RMS, SNR | `recalculateCalibrationChannel` (unchanged; tested identical) |
| `capture_emg.m` | Record N seconds with a live plot | `captureDualEMG` |
| `live_emg_plot.m` | The live EMG plot used while recording and viewing | Plot setup from 0B/1A/1B |
| `counts_to_volts.m` | `counts/1023*5 - 2.5` | The conversion used everywhere in his code |
| `emg_ctrl_bridge.m` | The live drive loop and drive window | Section 2, plus sending to the car |
| `run_controller.m` | Calls any controller with the same inputs | The controller call in Section 2 |
| `mappings/*.m` | Controller output → drive command (see below) | New |
| `parse_emg_chunk.m` | Parses the Uno's serial text | Replaces his `strsplit`/`sscanf` loop |
| `kirby_matlab_code/` | **Kirby's original code, unchanged.** The controllers are called from here | |
| `saved_data/` | Calibration `.mat` files (not in git: they are recordings of a person) | Same format as his `saved_data` |

Calibration files are interchangeable with Kirby's: same `calibration` struct, same `.mat` format.

## Controllers (Kirby's)

| Name | What it does | Sliders used |
|---|---|---|
| `control03b` (default) | Smoothed proportional: 250 ms RMS → 0 below threshold, else scaled between rest and contraction RMS → smoothed per channel | Sensitivity, Speed, Baseline |
| `control01` | Proportional, no smoothing | none |
| `control04` | Binary: 1 when RMS > threshold × Sensitivity | Sensitivity |

Not offered: `control02` and `control03`. Each keeps one memory shared by both channels, so with two
muscles each channel corrupts the other. `control03b` is the per-channel version of `control03`.
`control02` could be fixed the same way if the team wants it.

Controllers run every 20 ms, the rate Kirby's loop ran them at. That matters for control03b, whose
smoothing is per call.

## Mappings (controller output → car)

The car takes on/off bits: FWD (speeds up while held, cruises when released), SLOW (slows to 0),
LEFT/RIGHT (steers while held, re-centres when released) and E-STOP. A channel counts as **active**
when its controller output is above the **Activation level** slider.

| File | A0 (Channel 1) | A1 (Channel 2) | Both |
|---|---|---|---|
| `map_go_brake` (default) | FWD | SLOW | SLOW (brake wins) |
| `map_prop_go_brake` | FWD on a share of ticks = output (0.5 → half-rate acceleration) | SLOW, same idea | SLOW |
| `map_steer` | LEFT | RIGHT | FWD (no EMG brake: use E-STOP) |

## Swapping and adding

- **Order and defaults:** edit the two lists at the top of `run_emg_car.m`. The first entry is the
  default.
  ```matlab
  CONTROLLERS = ["control03b", "control01", "control04"];
  MAPPINGS    = ["map_go_brake", "map_prop_go_brake", "map_steer"];
  ```
- **New mapping:** copy `mappings/map_go_brake.m` to `mappings/map_mine.m`, keep the same inputs and
  outputs `[cmd, state] = map_mine(values, settings, state, KEY)`, and add `"map_mine"` to `MAPPINGS`.
- **New controller:** put the file in `kirby_matlab_code/`, add a `case` in `run_controller.m`, and add
  its name to `CONTROLLERS`. If it keeps `persistent` memory, it must keep it per channel and also be
  listed in `KNOWN` in `run_controller.m` so it is reset at the start of each drive.

## Tuning

- **Slider start values:** `SENSITIVITY_START`, `SPEED_START`, `BASELINE_START` and `LEVEL_START` at
  the top of `emg_ctrl_bridge.m`.
- **Slow laptop** (the printed command gap is over ~150 ms): redraw less often. Change `PLOT_PERIOD_S`
  at the top of `emg_ctrl_bridge.m` from 0.1 to 0.25.
- **Recording protocol** (5 s rest, 2 s contract and relax): `new_calibration` and
  `record_contractions` in `emg_calibrate.m`.

## Tested (without hardware)

The following were checked in MATLAB R2026a with simulated Uno and Nano boards, which stream
synthetic EMG in real time and record every byte sent:
- Code Analyzer: 0 messages on every file.
- Calibration math identical to Kirby's original function.
- Controllers through `run_controller` identical to calling them directly.
- All three mappings.
- Full drive sessions:
  - Dry run sends only E-STOP; LIVE starts stopped until RESUME.
  - A0 burst → FWD, A1 burst → SLOW, steer mapping LEFT/RIGHT/FWD, proportional mapping in between.
  - E-STOP button, auto-stop when the Uno goes quiet, and E-STOP ×5 on close.
  - Commands about every 50 ms.

Still to do on the real hardware: calibration prompts and dialogs, slider and button feel, and
drawing speed on your laptop.
