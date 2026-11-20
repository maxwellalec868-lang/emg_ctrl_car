"""
pc_drive.py - Keyboard -> Arduino (nrf_tx_drive.ino) -> nRF -> STM32 (full_drive_demo.c).

Every 50 ms sends ONE byte: a bitmask of the keys held right now.
  W = forward, S = slow/stop, A = left, D = right, Space = EMERGENCY STOP, Esc = e-stop and quit.

Setup:  pip install pyserial keyboard
Run:    python pc_drive.py COM5          (the Arduino's port; close the Serial Monitor first)

NOTE: the keyboard library reads keys GLOBALLY, even when another window has focus.
If this script stops for any reason, the STM32 stops the car on its own after 200 ms.
"""
import sys
import time

import keyboard
import serial

# Must match the KEY_xxx defines in stm_car_proj/Demos/full_drive_demo.c
KEYS = {"w": 0x01, "s": 0x02, "a": 0x04, "d": 0x08, "space": 0x10}
KEY_ESTOP = 0x10
SEND_PERIOD_S = 0.05

port = serial.Serial(sys.argv[1], 115200)
print("Driving. W/S/A/D, Space = e-stop, Esc = e-stop and quit.")

try:
    while not keyboard.is_pressed("esc"):
        mask = 0
        for key, bit in KEYS.items():
            if keyboard.is_pressed(key):
                mask |= bit
        port.write(bytes([mask]))
        time.sleep(SEND_PERIOD_S)
finally:
    for _ in range(5):  # e-stop on Esc, Ctrl+C or a crash
        port.write(bytes([KEY_ESTOP]))
        time.sleep(SEND_PERIOD_S)
    port.close()
