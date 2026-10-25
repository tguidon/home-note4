#!/usr/bin/env python3
"""Export bounded power diagnostics over USB, without saving application logs."""
import argparse
import json
import pathlib
import time
import serial


def capture(port, timeout=75):
    deadline, next_request = time.monotonic() + timeout, 0
    records, active = [], False
    with serial.Serial(port=None, baudrate=115200, timeout=0.5) as device:
        device.dtr = device.rts = False
        device.port = port
        device.open()
        while time.monotonic() < deadline:
            if time.monotonic() >= next_request and not active:
                device.write(b"\npower\n")
                next_request = time.monotonic() + 8
            line = device.readline(4096).decode("utf-8", errors="replace").strip()
            if line == "POWER_BEGIN,1":
                records, active = [], True
            elif active and line.startswith("POWER_STATS,"):
                record = json.loads(line.removeprefix("POWER_STATS,"))
                if record.get("schema") != 1 or record.get("kind") not in {"current", "latest", "previous", "discharge"}:
                    raise ValueError("Unrecognized power diagnostics")
                if any(r["kind"] == record["kind"] for r in records):
                    raise ValueError("Duplicate power snapshot")
                records.append(record)
            elif active and line == "POWER_END,1":
                if not records or records[0]["kind"] != "current":
                    raise ValueError("Incomplete power diagnostics")
                return records
    raise TimeoutError("No complete power diagnostics received within 75 seconds")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    records = capture(args.port)
    with args.output.open("x") as output:
        json.dump(records, output, indent=2)
        output.write("\n")
    print(f"Exported {len(records)} power snapshots to {args.output}")
