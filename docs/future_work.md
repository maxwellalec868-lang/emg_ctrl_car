# Future work / open TODOs

Notes only. This folder is outside `stm_car_proj/`, so nothing here affects building or flashing.

## 1. Check the EMDM I2C pull-ups (do this when the multimeter is available)

**Why:** I2C lines are open-drain. Devices only pull SDA/SCL *low*, so a pull-up resistor must pull them high.
- `GPIO_PU` in the demo turns on the STM32's **internal** pull-up. It is weak (about 40 kΩ), so edges rise slowly. That is out of spec at 100 kHz, but it often works with short wires.
- So "it worked before with `GPIO_PU`" does **not** prove the EMDM has its own pull-ups.

**How:**
1. Power the EMDM. **Disconnect** its SDA/SCL wires from the Nucleo.
2. Measure SDA→GND and SCL→GND.

**Result → what to change** (in `Demos/demo_emdm.c`, `emdm_bringup()`, the `EMDM_I2C_PUPD` define):
| Reading | Meaning | Setting |
|---|---|---|
| about 5 V | EMDM pulls up to 5 V | `GPIO_NO_PUPD`. The STM32 datasheet requires internal pull-ups to be **off** on a 5 V-tolerant pin that sees more than VDD + 0.3 V. |
| about 3.3 V | EMDM pulls up to 3.3 V | Keep `GPIO_PU` (harmless in parallel) |
| about 0 V / floating | No pull-ups on the board | Keep `GPIO_PU`. Adding 4.7 kΩ to 3.3 V on each line would be more robust. |

Current setting: **`GPIO_PU`** (not yet verified).

## 2. Radio failsafe / heartbeat (deferred)
Right now the car **keeps its last speed if the radio link drops**.
- STM32 side: if the speed is not 0 and no valid packet arrives for about 500 ms, call `EMDM_Stop()`.
- Arduino side: send a periodic `HEARTBEAT` command (e.g. `0x00`) while idle, so that silence means "link lost" rather than "no button pressed".

## 3. I2C polling timeouts
The `I2C_Master_TX` / `I2C_Master_RX` busy-wait loops have no timeout, so a stuck bus (e.g. SDA held low) hangs the program forever. Add a tick-based timeout that returns an error.

## 4. Speed ramp in the app layer
"Gradually go to speed X" needs a clock, so it belongs above the driver: every N ms, call `EMDM_Change_Drive_Speed()` by a small step until the target is reached.

## 5. Arduino command sender
`demo_emdm_radio_run()` expects packet byte 0 = command (`0x01` FORWARD, `0x02` SLOW, `0x03` STOP) and byte 1 = step size (0 = default of 5). The `nrf_tx_0x45` sketch still sends `0x45`, which the demo ignores. Move the command IDs into a header shared with the Arduino once that side is written.
