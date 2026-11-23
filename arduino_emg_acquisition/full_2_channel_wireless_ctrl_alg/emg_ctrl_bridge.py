"""
emg_ctrl_bridge.py - PC side of full_2_channel_wireless_ctrl_alg.ino.

  Arduino Uno --USB "AAAA BBBB" lines @ 1 kHz--> this script --control_algorithm()--> 1 byte --USB--> Arduino --nRF--> STM32

Every CMD_PERIOD_S this sends ONE byte, the same bitmask pc_drive.py sends:
  0x01 forward, 0x02 slow/stop, 0x04 left, 0x08 right, 0x10 EMERGENCY STOP
STM32 side: demo_full_PC_car_ctrl() in stm_car_proj/Demos/full_drive_demo.c (unchanged).

control_algorithm() below is a PLACEHOLDER (envelope + threshold) so the full chain can be tested.
Replace its body with the real algorithm. Keep the same inputs and output.

Setup:  pip install pyserial
Run:    python emg_ctrl_bridge.py COM5           (the Uno's port; close the Arduino Serial Monitor first)
        python emg_ctrl_bridge.py COM5 --dry-run (print decisions, send only E-STOP: wheels never move)

Ctrl+C sends E-STOP 5 times, then quits. If this script dies, the STM32 stops the car on its own after 200 ms.
"""
import argparse
import collections
import time

import serial

# Must match full_2_channel_wireless_ctrl_alg.ino
BAUD_RATE = 250000
N_CHANS = 2
SAMPLING_FREQ = 1000

# Must match the KEY_xxx defines in stm_car_proj/Demos/full_drive_demo.c
KEY_FWD = 0x01
KEY_SLOW = 0x02
KEY_LEFT = 0x04
KEY_RIGHT = 0x08
KEY_ESTOP = 0x10

CMD_PERIOD_S = 0.05  # one decision per STM32 control tick (50 ms). Must stay well under the 200 ms failsafe

# ---- Placeholder algorithm settings ----
WINDOW_S = 0.1            # envelope window
THRESHOLD = 40.0          # ADC counts of mean absolute deviation. Tune per person by watching the printout


def control_algorithm(window):
    """window: deque of (ch0, ch1) tuples, the newest WINDOW_S of raw ADC samples (0..1023).
    Returns the 1-byte command bitmask.

    PLACEHOLDER: envelope = mean |x - mean(x)| per channel.
      ch0 (A0, bottom shield) active -> forward
      ch1 (A1, top shield) active    -> steer right
      both active                    -> slow down
    """
    if len(window) < 10:
        return KEY_ESTOP
    env = []
    for ch in range(N_CHANS):
        xs = [s[ch] for s in window]
        mean = sum(xs) / len(xs)
        env.append(sum(abs(x - mean) for x in xs) / len(xs))

    active0, active1 = env[0] > THRESHOLD, env[1] > THRESHOLD
    if active0 and active1:
        cmd = KEY_SLOW
    elif active0:
        cmd = KEY_FWD
    elif active1:
        cmd = KEY_RIGHT
    else:
        cmd = 0
    control_algorithm.last_env = env  # for the status print only
    return cmd


control_algorithm.last_env = [0.0] * N_CHANS


def parse_line(raw):
    """b'0512 0498\\r\\n' -> (512, 498). Returns None for status lines ('#...') and partial lines."""
    line = raw.decode("ascii", errors="ignore").strip()
    if not line:
        return None
    if line.startswith("#"):
        print("[arduino]", line)
        return None
    parts = line.split()
    if len(parts) != N_CHANS:
        return None
    try:
        sample = tuple(int(p) for p in parts)
    except ValueError:
        return None
    if not all(0 <= v <= 1023 for v in sample):
        return None
    return sample


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="Arduino Uno serial port, e.g. COM5 or /dev/ttyACM0")
    ap.add_argument("--dry-run", action="store_true", help="compute commands but always send E-STOP")
    args = ap.parse_args()

    port = serial.Serial(args.port, BAUD_RATE, timeout=0.1)
    time.sleep(2.0)            # opening the port resets the Uno; wait for its bootloader to finish
    port.reset_input_buffer()

    window = collections.deque(maxlen=int(WINDOW_S * SAMPLING_FREQ))
    n_samples = 0
    last_cmd_t = time.monotonic()
    last_print_t = last_cmd_t
    cmd = KEY_ESTOP

    print("Running. Ctrl+C = E-STOP and quit." + ("  (DRY RUN: only E-STOP is sent)" if args.dry_run else ""))
    try:
        while True:
            sample = parse_line(port.readline())
            if sample is not None:
                window.append(sample)
                n_samples += 1

            now = time.monotonic()
            if now - last_cmd_t >= CMD_PERIOD_S:
                last_cmd_t = now
                cmd = control_algorithm(window)
                port.write(bytes([KEY_ESTOP if args.dry_run else cmd & 0x1F]))

            if now - last_print_t >= 1.0:
                env = control_algorithm.last_env
                print(f"{n_samples / (now - last_print_t):6.0f} samples/s   env={env[0]:6.1f} {env[1]:6.1f}   cmd=0x{cmd:02X}")
                n_samples = 0
                last_print_t = now
    except KeyboardInterrupt:
        pass
    finally:
        for _ in range(5):     # e-stop on Ctrl+C or a crash
            port.write(bytes([KEY_ESTOP]))
            time.sleep(CMD_PERIOD_S)
        port.close()


if __name__ == "__main__":
    main()
