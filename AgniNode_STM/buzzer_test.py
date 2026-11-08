#!/usr/bin/env python3
"""Periodically toggle the STM32 buzzer using its framed UART command protocol."""

import argparse
import sys
import time

import serial


def send_buzzer_command(port, sequence, state, duration=None):
    payload = f"CMD,{sequence},BUZZER,{state}"
    if duration is not None:
        payload += f",{duration}"
    checksum = 0
    for byte in payload.encode("ascii"):
        checksum ^= byte
    port.write(f"${payload}*{checksum:02X}\r\n".encode("ascii"))

    deadline = time.monotonic() + 2.0
    expected = f"$ACK,{sequence},BUZZER,"
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        if not line.startswith(expected):
            continue
        print(line)
        if ",OK," not in line:
            raise RuntimeError(f"STM32 rejected buzzer {state} command: {line}")
        return
    raise TimeoutError(f"No ACK received for buzzer {state} command (sequence {sequence})")


def positive_seconds(value):
    seconds = int(value)
    if seconds < 1:
        raise argparse.ArgumentTypeError("must be at least 1 second")
    return seconds


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="USB-UART COM port, for example COM5")
    parser.add_argument("--on-seconds", type=positive_seconds, default=2)
    parser.add_argument("--off-seconds", type=positive_seconds, default=2)
    parser.add_argument(
        "--cycles", type=positive_seconds, help="stop after this many ON/OFF cycles"
    )
    args = parser.parse_args()

    if args.on_seconds > 600:
        parser.error("--on-seconds cannot exceed the firmware limit of 600 seconds")

    sequence = time.monotonic_ns() & 0x7FFFFFFF
    cycles = 0
    try:
        with serial.Serial(args.port, baudrate=115200, timeout=0.2, write_timeout=2) as port:
            print(f"Connected to {args.port} at 115200 baud. Press Ctrl+C to stop.")
            try:
                while args.cycles is None or cycles < args.cycles:
                    sequence = sequence % 0xFFFFFFFF + 1
                    on_timeout = min(600, args.on_seconds + 2)
                    print(f"Requesting buzzer ON for {args.on_seconds} second(s)")
                    send_buzzer_command(port, sequence, "ON", on_timeout)
                    time.sleep(args.on_seconds)

                    sequence = sequence % 0xFFFFFFFF + 1
                    print("Requesting buzzer OFF")
                    send_buzzer_command(port, sequence, "OFF")
                    cycles += 1
                    if args.cycles is None or cycles < args.cycles:
                        time.sleep(args.off_seconds)
            except KeyboardInterrupt:
                print("\nStopping buzzer test.")
            finally:
                sequence = sequence % 0xFFFFFFFF + 1
                try:
                    print("Sending final buzzer OFF command")
                    send_buzzer_command(port, sequence, "OFF")
                except (serial.SerialException, TimeoutError, RuntimeError) as error:
                    print(f"WARNING: final OFF command was not confirmed: {error}", file=sys.stderr)
    except serial.SerialException as error:
        parser.exit(1, f"Could not use serial port {args.port}: {error}\n")


if __name__ == "__main__":
    main()
