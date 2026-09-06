#!/usr/bin/env python3
"""Read only battery telemetry over USB; no Wi-Fi credentials are exported."""
import argparse
import csv
import io
import pathlib
import time

import serial


def capture(port, timeout=75):
    deadline = time.monotonic() + timeout
    rows = []
    header = None
    next_request = 0
    with serial.Serial(port=None, baudrate=115200, timeout=0.5) as device:
        device.dtr = False
        device.rts = False
        device.port = port
        device.open()
        while time.monotonic() < deadline:
            if time.monotonic() >= next_request and header is None:
                device.write(b"\nbattery\n")
                next_request = time.monotonic() + 8
            line = device.readline().decode("ascii", errors="ignore").strip()
            # Other boot/application logs are deliberately neither saved nor printed.
            if line == "BATTERY_BEGIN,1":
                rows, header = [], None
            elif line.startswith("BATTERY_HEADER,"):
                header = next(csv.reader([line]))[1:]
            elif line.startswith("BATTERY_ROW,") and header:
                row = next(csv.reader([line]))[1:]
                if len(row) != len(header):
                    raise ValueError("Incomplete battery row; retry the export")
                values = [int(value) for value in row]
                if rows and values[0] != int(rows[-1][0]) + 1:
                    raise ValueError("Missing battery rows; retry the export")
                rows.append(row)
            elif line.startswith("BATTERY_END,") and header:
                if int(line.split(",")[1]) != len(rows):
                    raise ValueError("Incomplete battery export; retry")
                return header, rows
    raise TimeoutError("No complete battery export received within 75 seconds")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem2101")
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    header, rows = capture(args.port)
    content = io.StringIO(newline="")
    csv.writer(content).writerows([header, *rows])
    # Never replace an earlier measurement file accidentally.
    with args.output.open("x", newline="") as output:
        output.write(content.getvalue())
    print(f"Exported {len(rows)} battery readings to {args.output}")
